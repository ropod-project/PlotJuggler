#ifndef DIALOG_SELECT_ROS_TOPICS_H
#define DIALOG_SELECT_ROS_TOPICS_H

#include <QDialog>
#include <QString>
#include <QFile>
#include <QStringList>
#include <QCheckBox>
#include <QShortcut>
#include "PlotJuggler/optional.hpp"

namespace Ui {
class dialogSelectMongoDBCollections;
}

class DialogSelectMongoDBCollections : public QDialog
{
    Q_OBJECT

public:

    struct Configuration
    {
        QStringList selected_collections;
    };

    explicit DialogSelectMongoDBCollections(const std::vector<std::string>& collection_list,
                                   const Configuration& default_info,
                                   QWidget *parent = nullptr);

    ~DialogSelectMongoDBCollections() override;

    Configuration getResult() const;

public slots:

    void updateCollectionList(const std::vector<std::string> &collection_list);

private slots:

    void on_buttonBox_accepted();

    void on_listMongoDBCollections_itemSelectionChanged();

    void on_checkBoxEnableRules_toggled(bool checked);

    void on_pushButtonEditRules_pressed();

    void on_maximumSizeHelp_pressed();

    void on_lineEditFilter_textChanged(const QString &search_string);

    void on_spinBoxArraySize_valueChanged(int value);

private:

    QStringList _collection_list;
    QStringList _default_selected_collections;

    QShortcut _select_all;
    QShortcut _deselect_all;

    Ui::dialogSelectMongoDBCollections *ui;

};


#endif // DIALOG_SELECT_ROS_TOPICS_H
