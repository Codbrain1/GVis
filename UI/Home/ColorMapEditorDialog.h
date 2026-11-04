#pragma once

#include "Common/Structures/ColormapPresets.h"
#include <QDialog>
#include <optional>
#include <qtmetamacros.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class ColorMapEditorDialog;
}
QT_END_NAMESPACE

namespace QSpace::UI {
class ColorMapEditorDialog : public QDialog {
    Q_OBJECT
  public:
    enum class Mode { Create, Edit };

    explicit ColorMapEditorDialog(const QSpace::Visualize::ColorMap& baseMap,
                                  Mode                              mode,
                                  QWidget*                          parent = nullptr);
    ~ColorMapEditorDialog();
    Visualize::ColorMap getEditedMap() const;
  signals:
    void savePaletteRequested(const Visualize::ColorMap& map, const QString& filePath);
    void loadPaletteRequested(const QString& filePath);
  public slots:
    void onPaletteLoaded(QSpace::Visualize::ColorMap colorMap);
  private slots:
    void on_addButton_clicked();
    void on_removeButton_clicked();
    void on_colorTable_cellDoubleClicked(int row, int column);
    void on_loadButton_clicked();
    void on_saveButton_clicked();
    void on_invertButton_clicked();
    void on_spinBoxChanged(double value);

  private:
    Ui::ColorMapEditorDialog* ui;
    Visualize::ColorMap       m_baseMap;
    Mode                      m_mode;
    void                      populateTable();
};
} // namespace QSpace::UI