#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "Enums/IOEnums.h"
#include "Enums/VisualizeBaseEnums.h"
#include "IO/ReaderFactory.h"
#include <IO/SchemePresets.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
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

QSpace::IO::ReadResult readFile(const QString& path, const QSpace::IO::ReadScheme& scheme) {
    auto reader = QSpace::IO::createReader(QSpace::IO::FileFormat::BIN);
    reader->setPolicy(QSpace::IO::FilePolicy::ForceStandart);
    return reader->read(path, scheme);
}

void readFilesSequentially(const QFileInfoList&          files,
                           int                           count,
                           const QSpace::IO::ReadScheme& scheme) {
    for (int index = 0; index < count; ++index) {
        [[maybe_unused]] const auto result = readFile(files[index].absoluteFilePath(), scheme);
    }
}

void readFilesInParallel(const QFileInfoList&          files,
                         int                           count,
                         unsigned int                  maxThreads,
                         const QSpace::IO::ReadScheme& scheme) {
    std::vector<std::future<QSpace::IO::ReadResult>> tasks;
    tasks.reserve(std::min(count, static_cast<int>(maxThreads)));

    for (int index = 0; index < count; ++index) {
        tasks.emplace_back(std::async(std::launch::async, [&files, index, &scheme] {
            return readFile(files[index].absoluteFilePath(), scheme);
        }));

        if (tasks.size() == maxThreads) {
            for (auto& task : tasks) {
                [[maybe_unused]] const auto result = task.get();
            }
            tasks.clear();
        }
    }

    for (auto& task : tasks) {
        [[maybe_unused]] const auto result = task.get();
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

    const auto& scheme   = preset->second;
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

    qint64 totalBytes = 0;
    for (int count = 1; count <= files.size(); ++count) {
        totalBytes += files[count - 1].size();

        const auto start = std::chrono::steady_clock::now();
        if (options.mode == ExecutionMode::Sequential) {
            readFilesSequentially(files, count, scheme);
        } else {
            readFilesInParallel(files, count, options.threads, scheme);
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
    }

    return 0;
}
