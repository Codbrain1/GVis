#pragma once
#include "Common/Structures/ObjectRegistryStructures.h"
#include <QComboBox>
#include <QDialog>
#include <QRadioButton>
#include <QString>
#include <QUuid>
#include <qlineedit.h>
#include <qpushbutton.h>

struct SelectExperimentDialogResult {
    bool    isNewExperiment;   // true - создать новый эксперимент, false - добавить в существующий
    QString newExperimentName; // имя нового эксперимента или имя существующего эксперимента для добавления
    QUuid   exisitingExperimentId; // id существующего эксперимента для добавления, null если создаем новый
                                   // эксперимент
};

class SelectExperimentDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SelectExperimentDialog(const QList<std::shared_ptr<GVis::Core::Experiment>>& existingExperiments,
                                    QWidget*                                              parent);
    ~SelectExperimentDialog() = default;
    SelectExperimentDialogResult getResult() const;
  private slots:
    void handleRadioButtonsToggled();
    void handleValidateInut();

  private:
    QRadioButton* m_radioButtonExisting;
    QRadioButton* m_radioButtonNew;
    QLineEdit*    m_lineEditNewExperimentName;
    QPushButton*  m_buttonOk;
    QPushButton*  m_buttonCancel;
    QComboBox*    m_comboBoxExistingExperiments;
};