#ifndef QPDFPAGEMODEL_H
#define QPDFPAGEMODEL_H

#include <QtCore/qabstractitemmodel.h>
#include <QtGui/qimage.h>
#include <QPdfDocument> // 必须包含以调用 render 函数

QT_BEGIN_NAMESPACE

class QPdfPageModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit QPdfPageModel(QObject *parent = nullptr)
        : QAbstractListModel(parent), m_doc(nullptr) {}

    // 设置文档并刷新视图
    void setDocument(QPdfDocument *doc) {
        beginResetModel(); // 开始重置模型，通知视图刷新
        m_doc = doc;
        endResetModel();   // 结束重置
    }

    // 获取文档真实页数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid() || !m_doc) return 0;
        return m_doc->pageCount();
    }

    // 核心渲染逻辑：为视图提供缩略图和文字
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || !m_doc) return QVariant();

        int pageIndex = index.row();

        // 1. 提供缩略图 (QListView 在 IconMode 下会请求此 Role)
        if (role == Qt::DecorationRole) {
            // 渲染当前页为图片，建议尺寸 120x160
            return m_doc->render(pageIndex, QSize(120, 160));
        }

        // 2. 提供下方显示的页码文字
        if (role == Qt::DisplayRole) {
            return QString("第 %1 页").arg(pageIndex + 1);
        }

        // 3. 额外支持：居中对齐
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }

        return QVariant();
    }

private:
    QPdfDocument *m_doc;
};

QT_END_NAMESPACE

#endif // QPDFPAGEMODEL_H
