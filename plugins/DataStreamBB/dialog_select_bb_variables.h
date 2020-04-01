#ifndef DIALOG_SELECT_BB_VARIABLES_H
#define DIALOG_SELECT_BB_VARIABLES_H

#include <QDialog>
#include <QString>
#include <QFile>
#include <QStringList>
#include <QCheckBox>
#include <QShortcut>

/**
 * Adapted from plotJuggler's DialogSelectROSTopics class:
 */

namespace Ui {
class dialogSelectBBVariables;
}

class DialogSelectBBVariables : public QDialog
{
    Q_OBJECT

public:

    struct Configuration
    {
        QStringList selected_variables;
        size_t max_array_size;
        bool discard_large_arrays;
    };

    explicit DialogSelectBBVariables(const std::vector<std::pair<QString,QString>>& variable_list,
                                   const Configuration& default_info,
                                   QWidget *parent = nullptr);

    ~DialogSelectBBVariables() override;

    Configuration getResult() const;

public slots:

    void updateVariableList(std::vector<std::pair<QString,QString>> variable_list);

private slots:

    void on_buttonBox_accepted();

    void on_listBBVariables_itemSelectionChanged();

    void on_maximumSizeHelp_pressed();

    void on_lineEditFilter_textChanged(const QString &search_string);

    void on_spinBoxArraySize_valueChanged(int value);

private:

    QStringList _variable_list;
    QStringList _default_selected_variables;

    QShortcut _select_all;
    QShortcut _deselect_all;

    Ui::dialogSelectBBVariables *ui;

};


#endif // DIALOG_SELECT_BB_VARIABLES_H
