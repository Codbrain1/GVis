#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include "Enums/IOEnums.h"
#include "Enums/VisualizeBaseEnums.h"
#include "IO/ReaderFactory.h"
#include <IO/SchemePresets.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QCoreApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <путь_к_директории>\n";
        return 1;
    }

    QString dirPath = argv[1];
    QDir    targetDir(dirPath);

    if (!targetDir.exists()) {
        std::cerr << "Директория не найдена: " << dirPath.toStdString() << "\n";
        return 1;
    }

    // Фильтруем только файлы и сортируем их по возрастанию размера
    // (для сортировки по убыванию добавьте флаг QDir::Reversed)
    QFileInfoList fileList =
        targetDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Size);

    if (fileList.isEmpty()) {
        std::cout << "Директория пуста или не содержит файлов.\n";
        return 0;
    }

    auto reader = GVis::IO::createReader(GVis::IO::FileFormat::BIN);
    reader->setPolicy(GVis::IO::FilePolicy::ForceMapped);


    auto targetKey = GVis::IO::PresetKey{GVis::Visualize::EntityType::Stars,
                                         GVis::IO::FileFormat::BIN,
                                         GVis::IO::ModelingProgrammVersion::V2};

    const auto presets = GVis::IO::ReadSchemePresets::getStandardPresets();
    auto       it = std::find_if(presets.begin(), presets.end(), [&targetKey](const auto& pair) {
        return pair.first == targetKey;
    });

    if (it == presets.end()) {
        std::cerr << "Не найдена схема файла по его ключу\n";
        return 1;
    }

    const auto& scheme = it->second;


    constexpr int fileNameWidth = 60;
    constexpr int sizeWidth     = 14;
    constexpr int timeWidth     = 14;
    constexpr int speedWidth    = 16;
    constexpr int separatorSize = fileNameWidth + sizeWidth + timeWidth + speedWidth + 9;

    const std::string fileNameHeader = "Имя файла";
    std::cout << fileNameHeader
              << std::string(fileNameWidth - QString::fromUtf8("Имя файла").size(), ' ') << " | "
              << std::right << std::setw(sizeWidth) << "Размер (MB)" << " | "
              << std::setw(timeWidth) << "Время (ms)" << " | " << std::setw(speedWidth)
              << "Скорость (MB/s)" << "\n";
    std::cout << std::string(separatorSize, '-') << "\n";

    for (const QFileInfo& fileInfo : fileList) {
        QString absolutePath = fileInfo.absoluteFilePath();
        qint64  fileBytes    = fileInfo.size();
        double  sizeMB       = fileBytes / (1024.0 * 1024.0);

        auto start = std::chrono::high_resolution_clock::now();

        auto result = reader->read(absolutePath, scheme);

        auto                                      end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        double elapsedMs = elapsed.count();
        double speedMBs  = (elapsedMs > 0.0) ? (sizeMB / (elapsedMs / 1000.0)) : 0.0;

        QString fileName = fileInfo.fileName();
        if (fileName.size() > fileNameWidth) {
            fileName = fileName.left(fileNameWidth - 3) + "...";
        }

        std::cout << std::left << std::setw(fileNameWidth) << fileName.toStdString() << " | "
                  << std::right << std::fixed << std::setprecision(2) << std::setw(sizeWidth)
                  << sizeMB << " | " << std::setw(timeWidth) << elapsedMs << " | "
                  << std::setw(speedWidth) << speedMBs << "\n";
    }

    return 0;
}