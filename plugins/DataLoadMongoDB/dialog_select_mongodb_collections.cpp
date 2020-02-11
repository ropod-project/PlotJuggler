#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDebug>
#include <QMessageBox>
#include <QAbstractItemView>
#include <set>

#include "dialog_select_mongodb_collections.h"
#include "ui_dialog_select_mongodb_collections.h"


DialogSelectMongoDBCollections::DialogSelectMongoDBCollections(const std::vector<std::string> &collection_list,
                                             const Configuration &config,
                                             QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dialogSelectMongoDBCollections),
    _default_selected_collections(config.selected_collections),
    _select_all(QKeySequence(Qt::CTRL + Qt::Key_A), this),
    _deselect_all(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_A), this)
{

    auto flags = this->windowFlags();
    this->setWindowFlags( flags | Qt::WindowStaysOnTopHint);

    ui->setupUi(this);

    QStringList labels;
    labels.push_back("Topic name");
    labels.push_back("Datatype");

    ui->listMongoDBCollections->setHorizontalHeaderLabels(labels);
    ui->listMongoDBCollections->verticalHeader()->setVisible(false);

    updateCollectionList(collection_list);

    if( ui->listMongoDBCollections->rowCount() == 1)
    {
        ui->listMongoDBCollections->selectRow(0);
    }
    ui->listMongoDBCollections->setFocus();

    _select_all.setContext(Qt::WindowShortcut);
    _deselect_all.setContext(Qt::WindowShortcut);

    connect( &_select_all, &QShortcut::activated,
            ui->listMongoDBCollections, [this]()
            {
              for (int row=0; row< ui->listMongoDBCollections->rowCount(); row++)
              {
                if( !ui->listMongoDBCollections->isRowHidden(row) &&
                    !ui->listMongoDBCollections->item(row,0)->isSelected())
                {
                  ui->listMongoDBCollections->selectRow(row);
                }
              }
            });

    connect( &_deselect_all, &QShortcut::activated,
             ui->listMongoDBCollections, &QAbstractItemView::clearSelection );

    on_spinBoxArraySize_valueChanged( ui->spinBoxArraySize->value() );

    QSettings settings;
    restoreGeometry(settings.value("DialogSelectMongoDBCollections.geometry").toByteArray());

}

void DialogSelectMongoDBCollections::updateCollectionList(const std::vector<std::string> &collection_list )
{
    ui->listMongoDBCollections->clear();
    for (int i = 0; i < collection_list.size(); i++)
    {
        int num_rows = ui->listMongoDBCollections->rowCount();
        ui->listMongoDBCollections->insertRow(num_rows);
        ui->listMongoDBCollections->setItem(num_rows, 0, new QTableWidgetItem(QString::fromStdString(collection_list[i])));
    }
    ui->listMongoDBCollections->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->listMongoDBCollections->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->listMongoDBCollections->sortByColumn(0, Qt::AscendingOrder);

}


DialogSelectMongoDBCollections::~DialogSelectMongoDBCollections()
{
    QSettings settings;
    settings.setValue("DialogSelectMongoDBCollections.geometry", saveGeometry());
    delete ui;
}

DialogSelectMongoDBCollections::Configuration DialogSelectMongoDBCollections::getResult() const
{
    Configuration config;
    config.selected_collections      = _collection_list;
    return config;
}


void DialogSelectMongoDBCollections::on_buttonBox_accepted()
{
    QModelIndexList selected_indexes = ui->listMongoDBCollections->selectionModel()->selectedIndexes();
    QString selected_collections;

    foreach(QModelIndex index, selected_indexes)
    {
        if(index.column() == 0){
            _collection_list.push_back( index.data(Qt::DisplayRole).toString() );
            selected_collections.append( _collection_list.back() ).append(" ");
        }
    }
}

void DialogSelectMongoDBCollections::on_listMongoDBCollections_itemSelectionChanged()
{
    QModelIndexList indexes = ui->listMongoDBCollections->selectionModel()->selectedIndexes();

    ui->buttonBox->setEnabled( indexes.size() > 0) ;
}


void DialogSelectMongoDBCollections::on_checkBoxEnableRules_toggled(bool checked)
{
    ui->pushButtonEditRules->setEnabled( checked );
}

void DialogSelectMongoDBCollections::on_pushButtonEditRules_pressed()
{
}


void DialogSelectMongoDBCollections::on_maximumSizeHelp_pressed()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Help");
    msgBox.setText("Maximum Size of Arrays:\n\n"
                   "If the size of an Arrays is larger than this maximum value, the entire array is skipped.\n\n"
                   "This parameter is used to prevent the user from loading HUGE arrays, "
                   "such as images, pointclouds, maps, etc.\n"
                   "The term 'array' refers to the array in a message field,\n\n"
                   " See http://wiki.ros.org/msg.\n\n"
                   "This is NOT about the duration of a time series!\n\n"
                   "MOTIVATION: pretend that a user tries to load a RGB image, which probably contains "
                   "a few millions pixels.\n"
                   "Plotjuggler would naively create a single time series for each pixel of the image! "
                   "That makes no sense, of course, and it would probably freeze your system.\n"
                   );
    msgBox.exec();
}

void DialogSelectMongoDBCollections::on_lineEditFilter_textChanged(const QString& search_string)
{
    int visible_count = 0;
    bool updated = false;

    QStringList spaced_items = search_string.split(' ');

    for (int row=0; row < ui->listMongoDBCollections->rowCount(); row++)
    {
        auto item = ui->listMongoDBCollections->item(row,0);
        QString name = item->text();
        int pos = 0;
        bool toHide = false;

        for (const auto& item: spaced_items)
        {
            if( !name.contains(item) )
            {
                toHide = true;
                break;
            }
        }
        ui->listMongoDBCollections->setRowHidden(row, toHide );
    }
}

void DialogSelectMongoDBCollections::on_spinBoxArraySize_valueChanged(int value)
{

}



