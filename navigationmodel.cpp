#include "navigationmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QUrl>

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
NavigationProxyModel::NavigationProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

// -----------------------------------------------------------------------------
// Map a filesystem path to a proxy model index using the source model.
// Returns an invalid QModelIndex if the source model is not a QFileSystemModel.
// -----------------------------------------------------------------------------
QModelIndex NavigationProxyModel::setRootIndexFromPath(QString path)
{
    QFileSystemModel *model = dynamic_cast<QFileSystemModel *>(this->sourceModel());
    if (model) {
        QModelIndex result = this->mapFromSource(model->index(path));
        return result;
    }
    return QModelIndex();
}

// -----------------------------------------------------------------------------
// Return the QFileInfo of the given proxy index, mapped back to the source.
// -----------------------------------------------------------------------------
QFileInfo NavigationProxyModel::getFileInfoMappedToSource(const QModelIndex &index)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return QFileInfo();

    QModelIndex sourceIndex = this->mapToSource(index);
    return model->fileInfo(sourceIndex);
}

// -----------------------------------------------------------------------------
// Return the QFileInfo directly from the source model for the given proxy index.
// -----------------------------------------------------------------------------
QFileInfo NavigationProxyModel::getFileInfo(const QModelIndex &index)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return QFileInfo();

    return model->fileInfo(index);
}

// -----------------------------------------------------------------------------
// Create a new empty file in the directory represented by the given index.
// The generated file name is returned through the "name" output parameter.
// -----------------------------------------------------------------------------
void NavigationProxyModel::createFileHandler(QModelIndex &index, QString &name)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex sourceIndex = this->mapToSource(index);

    QString fileName = QStringLiteral("Untitled");
    QString fileType = QStringLiteral(".txt");
    QString filePath;

    if (sourceIndex.isValid()) {
        // Use the directory of the selected item as the base path.
        filePath = QDir(model->filePath(sourceIndex)).absolutePath();
    } else {
        // Fall back to the model root path.
        filePath = QDir(model->rootPath()).absolutePath();
    }

    QFile file;
    // Ensure the generated file name is unique inside the target directory.
    uniqueFileName(file, fileName, fileType, filePath + QDir::separator());

    file.open(QIODevice::ReadWrite);
    file.close();

    name = fileName + fileType;
}

// -----------------------------------------------------------------------------
// Create a new folder in the directory represented by the given index.
// The generated folder name is returned through the "name" output parameter.
// -----------------------------------------------------------------------------
void NavigationProxyModel::createFolderHandler(QModelIndex &index, QString &name)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex sourceIndex = this->mapToSource(index);

    QString folderName = QStringLiteral("New Folder");
    QString folderPath;

    if (sourceIndex.isValid()) {
        QFileInfo info = model->fileInfo(sourceIndex);
        folderPath = info.absoluteFilePath();
    } else {
        folderPath = model->rootPath();
    }

    QDir dir(folderPath);
    // Ensure the folder name is unique inside the target directory.
    uniqueFolderName(dir, folderName, folderPath + QDir::separator());

    dir.mkdir(folderName);
    name = folderName;
}

// -----------------------------------------------------------------------------
// Move the file or folder at the given index to the system trash.
// QFile::moveToTrash() is cross-platform: it uses the Windows Recycle Bin,
// the XDG trash on Linux, and the macOS Trash.
// -----------------------------------------------------------------------------
void NavigationProxyModel::deleteFileFolderHandler(QModelIndex &index)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex sourceIndex = this->mapToSource(index);
    QFileInfo fileInfo = model->fileInfo(sourceIndex);

    if (!fileInfo.exists())
        return;

    // Cross-platform trash operation.
    // On Windows this uses the Recycle Bin, on Linux the XDG trash,
    // and on macOS the Finder Trash.
    QFile::moveToTrash(fileInfo.absoluteFilePath());
}

// -----------------------------------------------------------------------------
// Open the native file manager at the location of the given index.
// Uses QDesktopServices so it works on Windows, Linux and macOS.
// -----------------------------------------------------------------------------
void NavigationProxyModel::openLocationHandler(QModelIndex &index)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex sourceIndex = this->mapToSource(index);

    QString folderPath;

    if (!sourceIndex.isValid()) {
        folderPath = model->rootPath();
    } else {
        QFileInfo fileInfo = model->fileInfo(sourceIndex);
        if (!fileInfo.isDir()) {
            fileInfo.setFile(fileInfo.absoluteFilePath());
        }
        folderPath = fileInfo.absolutePath();
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

// -----------------------------------------------------------------------------
// Copy the absolute path of the given index into the system clipboard.
// -----------------------------------------------------------------------------
void NavigationProxyModel::copyFileFolderHandler(QModelIndex &index)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex sourceIndex = this->mapToSource(index);

    QString path;

    if (!sourceIndex.isValid()) {
        path = model->rootPath();
    } else {
        QFileInfo fileInfo = model->fileInfo(sourceIndex);
        path = fileInfo.absoluteFilePath();
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(path);
}

// -----------------------------------------------------------------------------
// Recursively collect all sub-directory absolute paths under the given index.
// -----------------------------------------------------------------------------
void NavigationProxyModel::createAllFoldersList(QModelIndex index, QStringList &listPath)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    QModelIndex rootIndex = this->mapToSource(index);
    QFileInfo info = model->fileInfo(rootIndex);
    QString path = info.absoluteFilePath();

    if (info.isDir()) {
        QDir dir(path);
        if (dir.isEmpty()) {
            return;
        } else {
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

            if (model->hasChildren(rootIndex)) {
                QDirIterator di(path, QDir::Dirs, QDirIterator::Subdirectories);
                while (di.hasNext()) {
                    di.next();
                    if (di.fileInfo().isDir()) {
                        listPath.append(di.fileInfo().absoluteFilePath());
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Recursively collect all file absolute paths under the given index.
// -----------------------------------------------------------------------------
void NavigationProxyModel::createAllFilesList(QModelIndex index, QStringList &listPath)
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return;

    listPath.clear();

    QModelIndex rootIndex = this->mapToSource(index);
    QFileInfo info = model->fileInfo(rootIndex);
    QString path = info.absoluteFilePath();

    if (info.isDir()) {
        QDir dir(path);
        if (dir.isEmpty()) {
            return;
        } else {
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

            if (model->hasChildren(rootIndex)) {
                QDirIterator di(path, QDir::Files, QDirIterator::Subdirectories);
                while (di.hasNext()) {
                    di.next();
                    if (di.fileInfo().isFile()) {
                        listPath.append(di.fileInfo().absoluteFilePath());
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Recursively find a unique file name by appending "_new" until no conflict.
// -----------------------------------------------------------------------------
void NavigationProxyModel::uniqueFileName(QFile &file, QString &name, QString &type, const QString &path)
{
    file.setFileName(path + name + type);
    if (file.exists()) {
        name = name + QStringLiteral("_new");
        uniqueFileName(file, name, type, path);
    }
}

// -----------------------------------------------------------------------------
// Recursively find a unique folder name by appending "_new" until no conflict.
// -----------------------------------------------------------------------------
void NavigationProxyModel::uniqueFolderName(QDir &dir, QString &name, const QString &path)
{
    dir.setPath(path + name);
    if (dir.exists()) {
        name = name + QStringLiteral("_new");
        uniqueFolderName(dir, name, path);
    }
}

// -----------------------------------------------------------------------------
// Filter callback: decide whether a row should be shown by the proxy model.
// Rows outside the model root path are always accepted so the tree structure
// above the root remains intact.
// -----------------------------------------------------------------------------
bool NavigationProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QFileSystemModel *model = qobject_cast<QFileSystemModel *>(this->sourceModel());
    if (!model)
        return false;

    QModelIndex childIndex = model->index(source_row, 0, source_parent);

    // Empty filter pattern: accept everything.
    if (filterRegularExpression().pattern().isEmpty())
        return true;

    QString rootPath = model->rootPath();
    QString infoPath = model->fileInfo(childIndex).absoluteFilePath();

    // Always keep rows that are not under the root path, so that the parent
    // directory chain remains visible.
    if (!isSubdirectory(infoPath, rootPath))
        return true;

    return filterChildIndex(model, source_row, childIndex);
}

// -----------------------------------------------------------------------------
// Recursive filter used to match either file names or folder names.
// A pattern starting with '/' means "search for folders only".
// -----------------------------------------------------------------------------
bool NavigationProxyModel::filterChildIndex(QFileSystemModel *model, int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_row);

    QFileInfo info = model->fileInfo(source_parent);
    QString fileName = info.fileName().toLower();

    const QString pattern = filterRegularExpression().pattern();

    // Check whether the pattern is a folder-only search (starts with '/').
    if (!pattern.isEmpty() && pattern.at(0) == QLatin1Char('/')) {
        QString folderName = pattern.mid(1).toLower();

        if (info.isDir()) {
            if (fileName.contains(folderName)) {
                return true;
            }

            QDir dir(info.absoluteFilePath());
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
            int rowCount = dir.count();

            for (int row = 0; row < rowCount; row++) {
                QModelIndex childIndex = model->index(row, 0, source_parent);
                if (filterChildIndex(model, row, childIndex)) {
                    return true;
                }
            }
        }

        // Also match if any parent folder in the path contains the pattern.
        if (info.absolutePath().toLower().contains(folderName)) {
            return true;
        }
    } else {
        // Regular file search.
        if (info.isFile()) {
            if (fileName.contains(pattern)) {
                return true;
            } else {
                return false;
            }
        }

        if (info.isDir()) {
            QDir dir(info.absoluteFilePath());
            if (dir.isEmpty()) {
                return false;
            } else {
                dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                int rowCount = dir.count();

                for (int row = 0; row < rowCount; row++) {
                    QModelIndex childIndex = model->index(row, 0, source_parent);
                    if (filterChildIndex(model, row, childIndex)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// Check whether subDirPath is located under parentDirPath.
// Uses absolute paths so the comparison is platform independent.
// -----------------------------------------------------------------------------
bool NavigationProxyModel::isSubdirectory(const QString &subDirPath, const QString &parentDirPath) const
{
    QDir subDir(subDirPath);
    QDir parentDir(parentDirPath);

    QString absoluteSubDirPath = subDir.absolutePath();
    QString absoluteParentDirPath = parentDir.absolutePath();

    return absoluteSubDirPath.startsWith(absoluteParentDirPath);
}
