#include "Pdfreader.h"
#include "./ui_Pdfreader.h"
#include "qpdfpagemodel.h"

#include <QFileDialog>      // 用于弹出文件选择框
#include <QPdfView>          // 用于设置显示模式
#include <QPdfPageNavigator>
#include <QPdfBookmarkModel>
#include <QPdfSearchModel>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QCloseEvent>

Pdfreader::Pdfreader(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Pdfreader)
{
    ui->setupUi(this);
    ui->zoomcomboBox->addItems({
        "50%", "75%", "100%", "125%", "150%", "200%", "适合宽度", "适合页面"
    });

    // 设置默认显示 100%
    ui->zoomcomboBox->setCurrentIndex(2);
    // 默认隐藏搜索输入框和搜索按钮 ---
    ui->searchlineEdit->setVisible(false);
    ui->search->setVisible(false);
    // 1. 初始化 PDF 文档对象
    m_document = new QPdfDocument(this);

    // 2. 将文档对象关联到你 UI 里的 QPdfView (对象名是 widget)
    ui->widget->setDocument(m_document);
    // --- 初始化目录模型 ---
    m_bookmarkModel = new QPdfBookmarkModel(this);
    m_bookmarkModel->setDocument(m_document); // 将模型与文档关联
    ui->treeView->setModel(m_bookmarkModel);  // 将模型设置给界面上的树状视图
    m_searchModel = new QPdfSearchModel(this);
    m_searchModel->setDocument(m_document); // 关联文档
    // 将搜索模型的结果高亮显示在视图上
    ui->widget->setSearchModel(m_searchModel);
    // 1. 初始化页模型
    m_pageModel = new QPdfPageModel(this);
    m_pageModel->setDocument(m_document); // 关联当前的 PDF 文档
    // 2. 配置 UI 上的 QListView (假设对象名是 thumbnailView)
    if (ui->thumbnailview) {
        ui->thumbnailview->setModel(m_pageModel);

        // 设置显示模式为图标模式，这样看起来才像缩略图
        ui->thumbnailview->setViewMode(QListView::IconMode);
        ui->thumbnailview->setIconSize(QSize(120, 160));    // 设置缩略图的大小
        ui->thumbnailview->setGridSize(QSize(140, 190));    // 设置每个格子的间距
        ui->thumbnailview->setResizeMode(QListView::Adjust); // 窗口缩放时自动调整位置
        ui->thumbnailview->setWordWrap(true);               // 允许文字换行（显示页码）
    }
    initDatabase();
    loadDatabase();
    // --- 点击目录跳转页面 ---
    connect(ui->treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {


        QVariant data = index.data(static_cast<int>(QPdfBookmarkModel::Role::Location));

            int page = index.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt();
            if (page >= 0) ui->widget->pageNavigator()->jump(page, QPointF(), 0);
    });
    // 初始状态下禁用 spinBox，等加载文件后再启用
   ui->spinBox->setEnabled(false);
    connect(ui->widget->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this, [this](int pageIndex) {
        ui->spinBox->blockSignals(true);      // 防止死循环
        ui->spinBox->setValue(pageIndex + 1); // 索引转页码
        ui->spinBox->blockSignals(false);
    });
    connect(ui->layout, &QToolButton::clicked, this, [this]() {
        // 检查当前状态：如果搜索框或侧边栏是显示的，则全部隐藏
        bool isVisible = ui->tabWidget->isVisible();

        // 1. 切换左侧目录栏的显示状态
        ui->tabWidget->setVisible(!isVisible);

        // 2. 隐藏搜索框和搜索按钮
        ui->searchlineEdit->setVisible(!isVisible);
        ui->search->setVisible(!isVisible);

        // 3. 隐藏页码跳转的 spinBox
        ui->spinBox->setVisible(!isVisible);
    });
    connect(ui->actionSearchToggle, &QToolButton::clicked, this, [this]() {
        // 检查下方搜索输入框当前是否可见
        bool isVisible = ui->searchlineEdit->isVisible();

        // 切换可见性
        ui->searchlineEdit->setVisible(!isVisible);
        ui->search->setVisible(!isVisible); // “搜索”汉字按钮也跟着同步显示/隐藏
    });
    connect(ui->zoomcomboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (!ui->widget) return;

        if (text == "适合宽度") {
            // 在 QPdfView 中设置缩放模式
            ui->widget->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        } else if (text == "适合页面") {
            ui->widget->setZoomMode(QPdfView::ZoomMode::FitInView);
        } else {
            // 处理百分比数字
            QString percent = text;
            percent.remove('%');
            qreal zoomFactor = percent.toDouble() / 100.0;

            // 先切换为自定义缩放，再设置系数
            ui->widget->setZoomMode(QPdfView::ZoomMode::Custom);
            ui->widget->setZoomFactor(zoomFactor);
        }
    });
}

Pdfreader::~Pdfreader()
{
    delete ui;
}
//创建数据库
void Pdfreader::initDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("reading_history.db");

    if (db.open()) {
        QSqlQuery query;
        query.exec("CREATE TABLE IF NOT EXISTS history ("
                   "path TEXT PRIMARY KEY, "
                   "page INTEGER, "
                   "zoom REAL, "
                   "last_time DATETIME)");
    }
}
//保存
void Pdfreader::saveToDatabase() {
    QString fullTitle = this->windowTitle();
    QString filePath = fullTitle.section(": ", 1);
    if (filePath.isEmpty()) filePath = fullTitle.section("：", 1);

    // 确保文档存在且有效
    if (filePath.isEmpty() || !m_document || m_document->status() != QPdfDocument::Status::Ready) {
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO history (path, page, zoom, last_time) "
                  "VALUES (:path, :page, :zoom, DATETIME('now', 'localtime'))");

    query.bindValue(":path", filePath);


    int currentPageIndex = ui->spinBox->value() - 1;
    query.bindValue(":page", currentPageIndex);

    query.bindValue(":zoom", ui->widget->zoomFactor());

    if (!query.exec()) {
        qDebug() << "数据库保存失败: " << query.lastError().text();
    }
}
//加载数据
void Pdfreader::loadDatabase() {
    QSqlQuery query("SELECT path, page, zoom FROM history ORDER BY last_time DESC LIMIT 1");

    if (!query.exec() || !query.next()) {
        qDebug() << "【加载诊断】没有找到历史记录，或数据库为空。";
        return;
    }

    QString lastPath = query.value(0).toString();
    int lastPage = query.value(1).toInt();
    qreal lastZoom = query.value(2).toReal();

    if (!QFile::exists(lastPath)) {
        qDebug() << "【加载诊断】严重错误：保存的文件路径已不存在！";
        return;
    }

    // 将恢复 UI 的逻辑打包成一个独立块
    auto restoreUI = [this, lastPage, lastZoom]() {

        // 1. 解锁连续滚动模式
        ui->widget->setPageMode(QPdfView::PageMode::MultiPage);

        // 2. 恢复缩放
        ui->widget->setZoomMode(QPdfView::ZoomMode::Custom);
        ui->widget->setZoomFactor(lastZoom);
        ui->zoomcomboBox->setEditText(QString("%1%").arg(qRound(lastZoom * 100)));

        // 3. 设置范围并解锁 SpinBox
        int total = m_document->pageCount();
        if (total > 0) {
            ui->spinBox->setEnabled(true);            // 彻底解锁！
            ui->spinBox->blockSignals(true);          // 临时屏蔽信号，防止引发二次跳转
            ui->spinBox->setRange(1, total);
            ui->spinBox->setValue(lastPage + 1);      // 同步数字
            ui->spinBox->blockSignals(false);
        }

        // 4. 执行跳转
        ui->widget->pageNavigator()->jump(lastPage, QPointF(), 0);
    };

    // 监听：等文档真正准备好后，再去执行 UI 恢复
    connect(m_document, &QPdfDocument::statusChanged, this, [restoreUI](QPdfDocument::Status status){
        if (status == QPdfDocument::Status::Ready) {
            restoreUI();
        }
    }, Qt::SingleShotConnection);

    m_document->load(lastPath);
    this->setWindowTitle("正在阅读: " + lastPath);

    // 保险机制：如果加载速度极快瞬间完成（通常发生在小文件），直接调用
    if (m_document->status() == QPdfDocument::Status::Ready) {
        restoreUI();
    }
}
// 实现“打开文件夹/文件”功能
void Pdfreader::on_action_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择 PDF 文件", "", "PDF Files (*.pdf)");
    if (filePath.isEmpty()) return;

    // --- 核心：在数据库中检索该路径 ---
    int savedPage = 0;       // 默认第 0 页
    qreal savedZoom = 1.0;   // 默认 100% 缩放

    QSqlQuery query;
    query.prepare("SELECT page, zoom FROM history WHERE path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        // 如果找到了匹配的路径，读取保存的值
        savedPage = query.value(0).toInt();
        savedZoom = query.value(1).toReal();
        qDebug() << "找到历史记录，准备恢复到页码：" << savedPage;
    } else {
        qDebug() << "新文件，将从第一页开始阅读。";
    }

    // 调用统一函数进行加载和状态恢复
    openFileAndRestore(filePath, savedPage, savedZoom);
}
// 补全 spinBox 数字改变时触发的翻页逻辑
void Pdfreader::on_spinBox_valueChanged(int arg1)
{
    // 1. 获取导航器对象指针
    QPdfPageNavigator *navigator = ui->widget->pageNavigator();

    if (navigator) {
        // 2. 调用 jump 函数
        // 参数 1: 页码索引 (从 0 开始，所以 spinBox 的值要减 1)
        // 参数 2: 跳转后的坐标位置 (默认使用空坐标 QPointF())
        // 参数 3: 缩放比例 (保持当前缩放传 0 即可)
        navigator->jump(arg1 - 1, QPointF(), 0);
    }
}

// 4. 放大
void Pdfreader::on_zoomin_clicked()
{
    // 设定一个最大缩放上限，例如 500% (5.0)
    if (ui->widget->zoomFactor() > 5.0) return;

    ui->widget->setZoomMode(QPdfView::ZoomMode::Custom);
    ui->widget->setZoomFactor(ui->widget->zoomFactor() * 1.2);

    // 同步更新下拉框的文字，显示当前的百分比
    int percent = qRound(ui->widget->zoomFactor() * 100);
    ui->zoomcomboBox->setEditText(QString("%1%").arg(percent));
}

// 5. 缩小
void Pdfreader::on_zoomout_clicked()
{
    // 设定一个最小缩放比例，例如 10% (0.1)，防止页面消失
    if (ui->widget->zoomFactor() < 0.1) return;

    ui->widget->setZoomMode(QPdfView::ZoomMode::Custom);
    ui->widget->setZoomFactor(ui->widget->zoomFactor() / 1.2);

    // 同步更新下拉框的文字
    int percent = qRound(ui->widget->zoomFactor() * 100);
    ui->zoomcomboBox->setEditText(QString("%1%").arg(percent));
}

//6.搜索
void Pdfreader::on_search_clicked()
{
    QString text = ui->searchlineEdit->text();

    if (text.isEmpty()) {
        m_searchModel->setSearchString("");
        return;
    }


    m_searchModel->setSearchString(text);


    if (m_searchModel->rowCount(QModelIndex()) > 0) {
        // 获取第一个结果的索引
        QModelIndex firstResult = m_searchModel->index(0, 0);

        // 获取结果所在页码并跳转
        int page = firstResult.data(static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
        ui->widget->pageNavigator()->jump(page, QPointF(), 0);
    }
}

void Pdfreader::openFileAndRestore(const QString &filePath, int targetPage, qreal targetZoom) {
    if (filePath.isEmpty()) return;

    m_document->disconnect(SIGNAL(statusChanged(QPdfDocument::Status)));

    connect(m_document, &QPdfDocument::statusChanged, this, [this, targetPage, targetZoom](QPdfDocument::Status status){
        if (status == QPdfDocument::Status::Ready) {
            // --- 刷新目录和缩略图模型 ---
            // 必须重新设置一次 document 才能强制模型重新解析新文件的目录结构
            ui->treeView->setModel(nullptr);
            m_bookmarkModel->setDocument(nullptr);    //
            m_bookmarkModel->setDocument(m_document); //
            ui->treeView->setModel(m_bookmarkModel);  //
            m_bookmarkModel->setDocument(m_document);
            m_pageModel->setDocument(m_document);

            // 2. 模式与 UI 恢复
            ui->widget->setPageMode(QPdfView::PageMode::MultiPage);

            // 3. 恢复页码
            int total = m_document->pageCount();
            if (total > 0) {
                ui->spinBox->setEnabled(true);
                ui->spinBox->setRange(1, total);
                int safePage = qBound(0, targetPage, total - 1);
                ui->widget->pageNavigator()->jump(safePage, QPointF(), 0);

                ui->spinBox->blockSignals(true);
                ui->spinBox->setValue(safePage + 1);
                ui->spinBox->blockSignals(false);
            }

            // 4. 恢复缩放 (在跳转之后执行更稳定)
            if (targetZoom > 0.1) {
                ui->widget->setZoomMode(QPdfView::ZoomMode::Custom);
                ui->widget->setZoomFactor(targetZoom);

                int percent = qRound(targetZoom * 100);
                ui->zoomcomboBox->blockSignals(true);
                ui->zoomcomboBox->setEditText(QString("%1%").arg(percent));
                ui->zoomcomboBox->blockSignals(false);
            }
        }
    });

    m_document->load(filePath);
    this->setWindowTitle("正在阅读: " + filePath);
}
void Pdfreader::closeEvent(QCloseEvent *event) {
    saveToDatabase(); // 触发保存逻辑
    QMainWindow::closeEvent(event); // 调用父类的关闭处理，确保窗口正常关闭
}
