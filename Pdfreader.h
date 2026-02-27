#ifndef PDFREADER_H
#define PDFREADER_H

#include <QMainWindow>
#include<QPdfDocument>
#include <QPdfBookmarkModel>
#include <QPdfSearchModel>
#include "qpdfpagemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Pdfreader;
}
QT_END_NAMESPACE

class Pdfreader : public QMainWindow
{
    Q_OBJECT

public:
    Pdfreader(QWidget *parent = nullptr);
    ~Pdfreader();

private slots:
    void on_action_triggered(); // 打开文件夹

    void on_spinBox_valueChanged(int arg1);

    void on_zoomin_clicked();

    void on_zoomout_clicked();

    void on_search_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::Pdfreader *ui;
    QPdfDocument *m_document; // PDF 文档对象指针
    QPdfBookmarkModel *m_bookmarkModel; //目录模型指针
    QPdfSearchModel *m_searchModel;
    QPdfPageModel *m_pageModel;
    void initDatabase();
   void saveToDatabase();
    void loadDatabase();
   void openFileAndRestore(const QString &filePath, int targetPage, qreal targetZoom);
};
#endif // PDFREADER_H
