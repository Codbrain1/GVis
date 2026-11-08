#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "Common/Interfaces/IReader.h"
#include "Enums/IOEnums.h"
#include "Enums/VisualizeBaseEnums.h"
#include "IO/ReaderFactory.h"
#include "IO/SchemePresets.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

enum class ExecutionMode { Sequential, Parallel };

struct CommandLineOptions {
    QString       directory;
    ExecutionMode mode    = ExecutionMode::Sequential;
    unsigned int  threads = std::max(1U, std::thread::hardware_concurrency());
};

void printUsage(const char* executable) {
    std::cerr << "Использование: " << executable
              << " <путь_к_директории> [--mode sequential|parallel] [--threads N]\n";
}

bool parseOptions(int argc, char* argv[], CommandLineOptions& options) {
    if (argc < 2) {
        return false;
    }

    options.directory = QString::fromLocal8Bit(argv[1]);
    for (int index = 2; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == "--mode" && index + 1 < argc) {
            const QString mode = QString::fromLocal8Bit(argv[++index]);
            if (mode == "sequential") {
                options.mode = ExecutionMode::Sequential;
            } else if (mode == "parallel") {
                options.mode = ExecutionMode::Parallel;
            } else {
                return false;
            }
        } else if (argument == "--threads" && index + 1 < argc) {
            bool               isValid = false;
            const unsigned int threads = QString::fromLocal8Bit(argv[++index]).toUInt(&isValid);
            if (!isValid || threads == 0) {
                return false;
            }
            options.threads = threads;
        } else {
            return false;
        }
    }

    return true;
}

std::vector<QString> extractPaths(const QFileInfoList& files) {
    std::vector<QString> paths;
    paths.reserve(files.size());
    for (const auto& fileInfo : files) {
        paths.push_back(fileInfo.absoluteFilePath());
    }
    return paths;
}

// Постоянный пул потоков, чтобы не тратить время на CreateThread и createReader внутри замера
class ParallelReaderPool {
  public:
    ParallelReaderPool(unsigned int                  maxThreads,
                       const std::vector<QString>&   paths,
                       const QSpace::IO::ReadScheme& scheme)
        : threadCount_(std::max(1U, maxThreads)), paths_(paths), scheme_(scheme) {
        workers_.reserve(threadCount_);
        for (unsigned int workerId = 0; workerId < threadCount_; ++workerId) {
            workers_.emplace_back([this, workerId] { workerLoop(workerId); });
        }
    }

    ~ParallelReaderPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cvStart_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void readFiles(int count, std::vector<std::optional<QSpace::IO::ReadResult>>& results) {
        std::unique_lock<std::mutex> lock(mutex_);
        currentCount_    = count;
        currentResults_  = &results;
        activeWorkers_   = std::min<unsigned int>(threadCount_, static_cast<unsigned int>(count));
        finishedWorkers_ = 0;
        nextIndex_.store(0, std::memory_order_relaxed);
        ++generation_;

        cvStart_.notify_all();
        cvDone_.wait(lock, [this] { return finishedWorkers_ == activeWorkers_; });
    }

  private:
    void workerLoop(unsigned int workerId) {
        auto reader = QSpace::IO::createReader(QSpace::IO::FileFormat::BIN);
        reader->setPolicy(QSpace::IO::FilePolicy::ForceStandart);

        std::uint64_t seenGeneration = 0;

        while (true) {
            int                                                 count   = 0;
            std::vector<std::optional<QSpace::IO::ReadResult>>* results = nullptr;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cvStart_.wait(lock, [this, seenGeneration] {
                    return stop_ || generation_ > seenGeneration;
                });

                if (stop_) {
                    return;
                }

                seenGeneration = generation_;
                if (workerId >= activeWorkers_) {
                    continue;
                }

                count   = currentCount_;
                results = currentResults_;
            }

            while (true) {
                const int index = nextIndex_.fetch_add(1, std::memory_order_relaxed);
                if (index >= count) {
                    break;
                }
                (*results)[index] = reader->read(paths_[index], scheme_);
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (++finishedWorkers_ == activeWorkers_) {
                    cvDone_.notify_one();
                }
            }
        }
    }

    unsigned int                                        threadCount_;
    const std::vector<QString>&                         paths_;
    const QSpace::IO::ReadScheme&                       scheme_;
    std::vector<std::thread>                            workers_;
    std::mutex                                          mutex_;
    std::condition_variable                             cvStart_;
    std::condition_variable                             cvDone_;
    std::atomic<int>                                    nextIndex_{0};
    std::uint64_t                                       generation_      = 0;
    int                                                 currentCount_    = 0;
    unsigned int                                        activeWorkers_   = 0;
    unsigned int                                        finishedWorkers_ = 0;
    bool                                                stop_            = false;
    std::vector<std::optional<QSpace::IO::ReadResult>>* currentResults_  = nullptr;
};

void readFilesSequentially(QSpace::IReader&                                    reader,
                           const std::vector<QString>&                         paths,
                           int                                                 count,
                           const QSpace::IO::ReadScheme&                       scheme,
                           std::vector<std::optional<QSpace::IO::ReadResult>>& results) {
    for (int index = 0; index < count; ++index) {
        results[index] = reader.read(paths[index], scheme);
    }
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QCoreApplication app(argc, argv);

    CommandLineOptions options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    QDir targetDir(options.directory);
    if (!targetDir.exists()) {
        std::cerr << "Директория не найдена: " << options.directory.toStdString() << "\n";
        return 1;
    }

    const QFileInfoList files =
        targetDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (files.isEmpty()) {
        std::cout << "Директория пуста или не содержит файлов.\n";
        return 0;
    }

    const QSpace::IO::PresetKey targetKey{QSpace::Visualize::EntityType::Stars,
                                          QSpace::IO::FileFormat::BIN,
                                          QSpace::IO::ModelingProgrammVersion::V2};
    const auto                  presets = QSpace::IO::ReadSchemePresets::getStandardPresets();
    const auto                  preset =
        std::find_if(presets.cbegin(), presets.cend(), [&targetKey](const auto& pair) {
            return pair.first == targetKey;
        });
    if (preset == presets.cend()) {
        std::cerr << "Не найдена схема файла по его ключу.\n";
        return 1;
    }

    const auto&                scheme = preset->second;
    const std::vector<QString> paths  = extractPaths(files);

    const char* modeName = options.mode == ExecutionMode::Sequential ? "sequential" : "parallel";
    std::cout << "Режим: " << modeName;
    if (options.mode == ExecutionMode::Parallel) {
        std::cout << ", потоков: " << options.threads;
    }
    std::cout << "\n\n";

    constexpr int countWidth    = 14;
    constexpr int sizeWidth     = 18;
    constexpr int timeWidth     = 14;
    constexpr int speedWidth    = 18;
    constexpr int separatorSize = countWidth + sizeWidth + timeWidth + speedWidth + 9;

    std::cout << std::right << std::setw(countWidth) << "Файлов" << " | " << std::setw(sizeWidth)
              << "Размер (MB)" << " | " << std::setw(timeWidth) << "Время (ms)" << " | "
              << std::setw(speedWidth) << "Скорость (MB/s)" << "\n"
              << std::string(separatorSize, '-') << "\n";

    // Инициализируем ридеры и пул потоков один раз до начала серии замеров
    auto sequentialReader = QSpace::IO::createReader(QSpace::IO::FileFormat::BIN);
    sequentialReader->setPolicy(QSpace::IO::FilePolicy::ForceStandart);

    std::optional<ParallelReaderPool> parallelPool;
    if (options.mode == ExecutionMode::Parallel) {
        parallelPool.emplace(options.threads, paths, scheme);
    }

    qint64 totalBytes = 0;
    for (int count = 1; count <= files.size(); ++count) {
        totalBytes += files[count - 1].size();

        // Контейнер под загруженные данные текущего шага (1..count файлов)
        std::vector<std::optional<QSpace::IO::ReadResult>> results(count);

        const auto start = std::chrono::steady_clock::now();
        if (options.mode == ExecutionMode::Sequential) {
            readFilesSequentially(*sequentialReader, paths, count, scheme, results);
        } else {
            parallelPool->readFiles(count, results);
        }
        const std::chrono::duration<double, std::milli> elapsed =
            std::chrono::steady_clock::now() - start;

        const double sizeMB    = totalBytes / (1024.0 * 1024.0);
        const double elapsedMs = elapsed.count();
        const double speedMBs  = elapsedMs > 0.0 ? sizeMB / (elapsedMs / 1000.0) : 0.0;

        std::cout << std::right << std::setw(countWidth) << count << " | " << std::fixed
                  << std::setprecision(2) << std::setw(sizeWidth) << sizeMB << " | "
                  << std::setw(timeWidth) << elapsedMs << " | " << std::setw(speedWidth) << speedMBs
                  << "\n";

        // Очистка памяти results происходит здесь — ПОСЛЕ остановки таймера
    }

    return 0;
}