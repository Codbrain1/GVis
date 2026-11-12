#include "SelectExperimentDialog.h"
#include <QDialogButtonBox>
#include <memory.h>
#include <memory>
#include <qboxlayout.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>

SelectExperimentDialog::SelectExperimentDialog(
    const QList<std::shared_ptr<GVis::Core::Experiment>>& existingExperiments,
    QWidget*                                              parent)
    : QDialog(parent) {
    setWindowTitle(tr("Выбор эксперимента"));
    QVBoxLayout* mainLayout          = new QVBoxLayout(this);
    QHBoxLayout* comboBoxLayout      = new QHBoxLayout();
    QHBoxLayout* newExperimentLayout = new QHBoxLayout();

    m_radioButtonExisting         = new QRadioButton(tr("Выбрать существующий эксперимент"), this);
    m_radioButtonNew              = new QRadioButton(tr("Создать новый эксперимент"), this);
    m_lineEditNewExperimentName   = new QLineEdit(this);
    m_comboBoxExistingExperiments = new QComboBox(this);

    for (const auto& experiment : existingExperiments) {
        m_comboBoxExistingExperiments->addItem(experiment->name, QVariant::fromValue(experiment->id));
    }

    if (existingExperiments.isEmpty()) {
        m_radioButtonExisting->setEnabled(false);
        m_comboBoxExistingExperiments->setEnabled(false);
        m_radioButtonNew->setChecked(true);
    } else {
        m_lineEditNewExperimentName->setEnabled(false);
        m_radioButtonExisting->setChecked(true);
    }
    m_lineEditNewExperimentName->setPlaceholderText(tr("Введите имя нового эксперимента..."));

    comboBoxLayout->addWidget(m_radioButtonExisting);
    comboBoxLayout->addWidget(m_comboBoxExistingExperiments);

    newExperimentLayout->addWidget(m_radioButtonNew);
    newExperimentLayout->addWidget(m_lineEditNewExperimentName);

    mainLayout->addLayout(comboBoxLayout);
    mainLayout->addLayout(newExperimentLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonOk                  = buttonBox->button(QDialogButtonBox::Ok);
    m_buttonCancel              = buttonBox->button(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttonBox);

    connect(m_radioButtonExisting,
            &QRadioButton::toggled,
            this,
            &SelectExperimentDialog::handleRadioButtonsToggled);
    connect(m_radioButtonNew,
            &QRadioButton::toggled,
            this,
            &SelectExperimentDialog::handleRadioButtonsToggled);
    connect(m_lineEditNewExperimentName,
            &QLineEdit::textChanged,
            this,
            &SelectExperimentDialog::handleValidateInut);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    handleRadioButtonsToggled();
    handleValidateInut();
}
void SelectExperimentDialog::handleRadioButtonsToggled() {
    bool isExisting = m_radioButtonExisting->isChecked();
    m_comboBoxExistingExperiments->setEnabled(isExisting);
    m_lineEditNewExperimentName->setEnabled(!isExisting);
}

void SelectExperimentDialog::handleValidateInut() {
    if (m_radioButtonNew->isChecked()) {
        m_buttonOk->setEnabled(!m_lineEditNewExperimentName->text().trimmed().isEmpty());
    } else {
        m_buttonOk->setEnabled(m_comboBoxExistingExperiments->currentIndex() != -1);
    }
}
SelectExperimentDialogResult SelectExperimentDialog::getResult() const {
    SelectExperimentDialogResult res;
    if (m_radioButtonExisting->isChecked()) {
        res.isNewExperiment       = false;
        res.exisitingExperimentId = m_comboBoxExistingExperiments->currentData().toUuid();
    } else {
        res.isNewExperiment   = true;
        res.newExperimentName = m_lineEditNewExperimentName->text().trimmed();
    }
    return res;
}