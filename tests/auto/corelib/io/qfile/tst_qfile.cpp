// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2021 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#define _CRT_SECURE_NO_WARNINGS 1

#include <QTest>
#include <QScopedValueRollback>
#include <qplatformdefs.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QOperatingSystemVersion>
#include <QRandomGenerator>
#include <QStorageInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <private/qabstractfileengine_p.h>
#include <private/qfsfileengine_p.h>
#include <private/qfilesystemengine_p.h>

#ifdef Q_OS_WIN
#include <QtCore/private/qfunctions_win_p.h>
#endif

#include <QtTest/private/qemulationdetector_p.h>

#ifdef Q_OS_WIN
QT_BEGIN_NAMESPACE
extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
QT_END_NAMESPACE
#endif

#if !defined(QT_NO_NETWORK)
#include <QHostInfo>
#endif
#if QT_CONFIG(process)
# include <QProcess>
#endif
#ifdef Q_OS_WIN
# include <qt_windows.h>
#else
# include <sys/types.h>
# include <unistd.h>
# include <private/qcore_unix_p.h>
#endif
#ifdef Q_OS_DARWIN
# include <sys/mount.h>
#elif defined(Q_OS_LINUX)
# include <sys/eventfd.h>
# include <sys/vfs.h>
# include <sys/wait.h>
#elif defined(Q_OS_FREEBSD)
# include <sys/param.h>
# include <sys/mount.h>
#elif defined(Q_OS_VXWORKS)
# include <fcntl.h>
#if defined(_WRS_KERNEL)
#undef QT_OPEN
#define QT_OPEN(path, oflag) ::open(path, oflag, 0)
#endif
#endif

#ifdef Q_OS_QNX
#ifdef open
#undef open
#endif
#endif

#include <stdio.h>
#include <errno.h>

#ifdef Q_OS_ANDROID
// Android introduces a braindamaged fileno macro that isn't
// compatible with the POSIX fileno or its own FILE type.
#  undef fileno
#endif

#include "../../../../shared/filesystem.h"

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef QT_OPEN_BINARY
#define QT_OPEN_BINARY 0
#endif

using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(QFile::FileError)


class StdioFileGuard
{
    Q_DISABLE_COPY(StdioFileGuard)
public:
    explicit StdioFileGuard(FILE *f = nullptr) : m_file(f) {}
    ~StdioFileGuard() { close(); }

    operator FILE *() const { return m_file; }

    void close();

private:
    FILE * m_file;
};

void StdioFileGuard::close()
{
    if (m_file != nullptr) {
        fclose(m_file);
        m_file = nullptr;
    }
}

class tst_QFile : public QObject
{
    Q_OBJECT
public:
    tst_QFile();

private slots:
    void init();
    void cleanup();
    void initTestCase();
    void cleanupTestCase();
    void exists();
    void open_data();
    void open();
    void openUnbuffered();
    void size_data();
    void size();
    void sizeNoExist();
    void seek();
    void setSize();
    void setSizeSeek();
    void atEnd();
    void readLine();
    void readLine2();
    void readLineNullInLine();
    void readAll_data();
    void readAll();
    void readAllBuffer();
    void readAllStdin();
    void readLineStdin();
    void readLineStdin_lineByLine();
    void text();
    void missingEndOfLine();
    void readBlock();
    void getch();
    void ungetChar();
    void createFile();
    void createFileNewOnly();
    void createFilePermissions_data();
    void createFilePermissions();
    void openFileExistingOnly();
    void append();
    void permissions_data();
    void permissions();
#ifdef Q_OS_WIN
    void permissionsNtfs_data();
    void permissionsNtfs();
#if QT_DEPRECATED_SINCE(6,6)
    void deprecatedNtfsPermissionCheck();
#endif
#endif
    void setPermissions_data();
    void setPermissions();
    void copy();
    void copyAfterFail();
    void copyRemovesTemporaryFile() const;
    void copyShouldntOverwrite();
    void copyFallback();
    void link();
    void linkToDir();
    void absolutePathLinkToRelativePath();
    void readBrokenLink();
    void readTextFile_data();
    void readTextFile();
    void readTextFile2();
    void writeTextFile_data();
    void writeTextFile();
    /* void largeFileSupport(); */
#if defined(Q_OS_WIN)
    void largeUncFileSupport();
#endif
    void flush();
    void bufferedRead();
#ifdef Q_OS_UNIX
    void isSequential_data();
    void isSequential();
#endif
    void decodeName_data();
    void decodeName();
    void encodeName_data() { decodeName_data(); }
    void encodeName();
    void truncate();
    void seekToPos();
    void seekAfterEndOfFile();
    void FILEReadWrite();
    void i18nFileName_data();
    void i18nFileName();
    void longFileName_data();
    void longFileName();
    void fileEngineHandler();
#ifdef QT_BUILD_INTERNAL
    void useQFileInAFileHandler();
#endif
    void getCharFF();
    void remove_and_exists();
    void removeOpenFile();
    void fullDisk();
    void writeLargeDataBlock_data();
    void writeLargeDataBlock();
    void readFromWriteOnlyFile();
    void writeToReadOnlyFile();
#if defined(Q_OS_LINUX)
    void virtualFile_data();
    void virtualFile();
#endif
#if defined(Q_OS_UNIX) && !defined(Q_OS_WASM)
    void unixPipe_data();
    void unixPipe();
    void unixFifo_data() { unixPipe_data(); }
    void unixFifo();
    void socketPair_data() { unixPipe_data(); }
    void socketPair();
#endif
    void textFile();
    void rename_data();
    void rename();
    void renameWithAtEndSpecialFile() const;
    void renameFallback();
    void renameMultiple();
    void appendAndRead();
    void miscWithUncPathAsCurrentDir();
    void standarderror();
    void handle();
    void nativeHandleLeaks();

    void readEof_data();
    void readEof();

    void map_data();
    void map();
    void mapResource_data();
    void mapResource();
    void mapOpenMode_data();
    void mapOpenMode();
    void mapWrittenFile_data();
    void mapWrittenFile();

    void openStandardStreamsFileDescriptors();
    void openStandardStreamsBufferedStreams();

    void resize_data();
    void resize();

    void objectConstructors();

    void caseSensitivity();

    void autocloseHandle();

    void posAfterFailedStat();

    void openDirectory();
    void writeNothing();

    void invalidFile_data();
    void invalidFile();

    void reuseQFile();

    void supportsMoveToTrash();
    void moveToTrash_data();
    void moveToTrash();
    void moveToTrashDuplicateName();
    void moveToTrashOpenFile_data();
    void moveToTrashOpenFile();
    void moveToTrashSymlinkToFile();
    void moveToTrashSymlinkToDirectory_data();
    void moveToTrashSymlinkToDirectory();
    void moveToTrashXdgSafety();

    void stdfilesystem();

private:
#ifdef BUILTIN_TESTDATA
    QSharedPointer<QTemporaryDir> m_dataDir;
#endif
    enum FileType {
        OpenQFile,
        OpenFd,
        OpenStream,
        NumberOfFileTypes
    };

    bool openFd(QFile &file, QIODevice::OpenMode mode, QFile::FileHandleFlags handleFlags)
    {
        int fdMode = QT_OPEN_LARGEFILE | QT_OPEN_BINARY;

        // File will be truncated if in Write mode.
        if (mode & QIODevice::WriteOnly)
            fdMode |= QT_OPEN_WRONLY | QT_OPEN_TRUNC;
        if (mode & QIODevice::ReadOnly)
            fdMode |= QT_OPEN_RDONLY;

        fd_ = QT_OPEN(qPrintable(file.fileName()), fdMode);

        return (-1 != fd_) && file.open(fd_, mode, handleFlags);
    }

    bool openStream(QFile &file, QIODevice::OpenMode mode, QFile::FileHandleFlags handleFlags)
    {
        char const *streamMode = "";

        // File will be truncated if in Write mode.
        if (mode & QIODevice::WriteOnly)
            streamMode = "wb+";
        else if (mode & QIODevice::ReadOnly)
            streamMode = "rb";

        stream_ = QT_FOPEN(qPrintable(file.fileName()), streamMode);

        return stream_ && file.open(stream_, mode, handleFlags);
    }

    bool openFile(QFile &file, QIODevice::OpenMode mode, FileType type = OpenQFile, QFile::FileHandleFlags handleFlags = QFile::DontCloseHandle)
    {
        if (mode & QIODevice::WriteOnly && !file.exists())
        {
            // Make sure the file exists
            QFile createFile(file.fileName());
            if (!createFile.open(QIODevice::ReadWrite))
                return false;
        }

        // Note: openFd and openStream will truncate the file if write mode.
        switch (type)
        {
            case OpenQFile:
                return file.open(mode);

            case OpenFd:
                return openFd(file, mode, handleFlags);

            case OpenStream:
                return openStream(file, mode, handleFlags);

            case NumberOfFileTypes:
                break;
        }

        return false;
    }

    void closeFile(QFile &file)
    {
        file.close();

        if (-1 != fd_)
            QT_CLOSE(fd_);
        if (stream_)
            ::fclose(stream_);

        fd_ = -1;
        stream_ = 0;
    }

    int fd_;
    FILE *stream_;

    QTemporaryDir m_temporaryDir;
    const QString m_oldDir;
    QString m_stdinProcess;
    QString m_testSourceFile;
    QString m_testLogFile;
    QString m_dosFile;
    QString m_forCopyingFile;
    QString m_forRenamingFile;
    QString m_twoDotsFile;
    QString m_testFile;
    QString m_resourcesDir;
    QString m_noEndOfLineFile;
};

static const char noReadFile[] = "noreadfile";
static const char readOnlyFile[] = "readonlyfile";

void tst_QFile::init()
{
    fd_ = -1;
    stream_ = 0;
}

void tst_QFile::cleanup()
{
    if (-1 != fd_)
        QT_CLOSE(fd_);
    fd_ = -1;
    if (stream_)
        ::fclose(stream_);
    stream_ = 0;

    // Windows UNC tests set a different working directory which might not be restored on failures.
    if (QDir::currentPath() != m_temporaryDir.path())
        QVERIFY(QDir::setCurrent(m_temporaryDir.path()));

    // Clean out everything except the readonly-files.
    const QDir dir(m_temporaryDir.path());
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString fileName = fi.fileName();
        if (fileName != QLatin1String(noReadFile) && fileName != QLatin1String(readOnlyFile)) {
            const QString absoluteFilePath = fi.absoluteFilePath();
            if (fi.isDir() && !fi.isSymLink()) {
                QDir remainingDir(absoluteFilePath);
                QVERIFY2(remainingDir.removeRecursively(), qPrintable(absoluteFilePath));
            } else {
                if (!(QFile::permissions(absoluteFilePath) & QFile::WriteUser))
                    QVERIFY2(QFile::setPermissions(absoluteFilePath, QFile::WriteUser), qPrintable(absoluteFilePath));
                QVERIFY2(QFile::remove(absoluteFilePath), qPrintable(absoluteFilePath));
            }
        }
    }
}

tst_QFile::tst_QFile() : m_oldDir(QDir::currentPath())
{
    QStandardPaths::setTestModeEnabled(true);
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
}

static QByteArray msgOpenFailed(QIODevice::OpenMode om, const QFile &file)
{
    QString result;
    QDebug(&result).noquote().nospace() << "Could not open \""
        << QDir::toNativeSeparators(file.fileName()) << "\" using "
        << om << ": " << file.errorString();
    return result.toLocal8Bit();
}

static QByteArray msgOpenFailed(const QFile &file)
{
    return (QLatin1String("Could not open \"") + QDir::toNativeSeparators(file.fileName())
        + QLatin1String("\": ") + file.errorString()).toLocal8Bit();
}

static QByteArray msgFileDoesNotExist(const QString &name)
{
    return (QLatin1Char('"') + QDir::toNativeSeparators(name)
        + QLatin1String("\" does not exist.")).toLocal8Bit();
}

void tst_QFile::initTestCase()
{
    QVERIFY2(m_temporaryDir.isValid(), qPrintable(m_temporaryDir.errorString()));
#if QT_CONFIG(process)
#if defined(Q_OS_ANDROID)
    m_stdinProcess = QCoreApplication::applicationDirPath() + QLatin1String("/libstdinprocess_helper.so");
#elif defined(Q_OS_WIN)
    m_stdinProcess = QFINDTESTDATA("stdinprocess_helper.exe");
#else
    m_stdinProcess = QFINDTESTDATA("stdinprocess_helper");
#endif
    QVERIFY(!m_stdinProcess.isEmpty());
#endif
    m_testLogFile = QFINDTESTDATA("testlog.txt");
    QVERIFY(!m_testLogFile.isEmpty());
    m_dosFile = QFINDTESTDATA("dosfile.txt");
    QVERIFY(!m_dosFile.isEmpty());
    m_forCopyingFile = QFINDTESTDATA("forCopying.txt");
    QVERIFY(!m_forCopyingFile .isEmpty());
    m_forRenamingFile = QFINDTESTDATA("forRenaming.txt");
    QVERIFY(!m_forRenamingFile.isEmpty());
    m_twoDotsFile = QFINDTESTDATA("two.dots.file");
    QVERIFY(!m_twoDotsFile.isEmpty());

#ifndef BUILTIN_TESTDATA
    m_testSourceFile = QFINDTESTDATA("tst_qfile.cpp");
    QVERIFY(!m_testSourceFile.isEmpty());
    m_testFile = QFINDTESTDATA("testfile.txt");
    QVERIFY(!m_testFile.isEmpty());
    m_resourcesDir = QFINDTESTDATA("resources");
    QVERIFY(!m_resourcesDir.isEmpty());
#else
    m_dataDir = QEXTRACTTESTDATA("/");
    QVERIFY2(!m_dataDir.isNull(), qPrintable("Could not extract test data"));
    m_testFile = m_dataDir->path() + "/testfile.txt";
    m_testSourceFile = m_dataDir->path() + "/tst_qfile.cpp";
    m_resourcesDir = m_dataDir->path() + "/resources";
#endif
    m_noEndOfLineFile = QFINDTESTDATA("noendofline.txt");
    QVERIFY(!m_noEndOfLineFile.isEmpty());

    QVERIFY(QDir::setCurrent(m_temporaryDir.path()));

    // create a file and make it read-only
    QFile file(QString::fromLatin1(readOnlyFile));
    QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
    file.write("a", 1);
    file.close();
    QVERIFY2(file.setPermissions(QFile::ReadOwner), qPrintable(file.errorString()));
    // create another file and make it not readable
    file.setFileName(QString::fromLatin1(noReadFile));
    QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
    file.write("b", 1);
    file.close();
#ifndef Q_OS_WIN // Not supported on Windows.
    QVERIFY2(file.setPermissions({ }), qPrintable(file.errorString()));
#else
    QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
#endif
}

void tst_QFile::cleanupTestCase()
{
    QFile file(QString::fromLatin1(readOnlyFile));
    QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
    file.setFileName(QString::fromLatin1(noReadFile));
    QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
    QVERIFY(QDir::setCurrent(m_oldDir)); //release test directory for removal
}

//------------------------------------------
// The 'testfile' is currently just a
// testfile. The path of this file, the
// attributes and the contents itself
// will be changed as far as we have a
// proper way to handle files in the
// testing environment.
//------------------------------------------

void tst_QFile::exists()
{
    QFile f( m_testFile );
    QVERIFY2(f.exists(), msgFileDoesNotExist(m_testFile));

    QFile file("nobodyhassuchafile");
    file.remove();
    QVERIFY(!file.exists());

    QFile file2("nobodyhassuchafile");
    QVERIFY2(file2.open(QIODevice::WriteOnly), msgOpenFailed(file2).constData());
    file2.close();

    QVERIFY(file.exists());

    QVERIFY2(file.open(QIODevice::WriteOnly), msgOpenFailed(file).constData());
    file.close();
    QVERIFY(file.exists());

    file.remove();
    QVERIFY(!file.exists());

#if defined(Q_OS_WIN)
    const QString uncPath = "//" + QTest::uncServerName() + "/testshare/readme.txt";
    QFile unc(uncPath);
    QVERIFY2(unc.exists(), msgFileDoesNotExist(uncPath).constData());
#endif

    QTest::ignoreMessage(QtWarningMsg, "Broken filename passed to function");
    QVERIFY(!QFile::exists(QDir::currentPath() + QLatin1Char('/') +
                           QChar(QChar::Null) + QLatin1String("x/y")));
}

void tst_QFile::open_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QFile::FileError>("status");

    QTest::newRow( "exist_readOnly"  )
        << m_testFile << int(QIODevice::ReadOnly)
        << true << QFile::NoError;

    QTest::newRow( "exist_writeOnly" )
        << QString::fromLatin1(readOnlyFile)
        << int(QIODevice::WriteOnly)
        << false
        << QFile::OpenError;

    QTest::newRow( "exist_append"    )
        << QString::fromLatin1(readOnlyFile) << int(QIODevice::Append)
        << false << QFile::OpenError;

    QTest::newRow( "nonexist_readOnly"  )
        << QString("nonExist.txt") << int(QIODevice::ReadOnly)
        << false << QFile::OpenError;

    QTest::newRow("emptyfile")
        << QString("")
        << int(QIODevice::ReadOnly)
        << false
        << QFile::OpenError;

    QTest::newRow("nullfile") << QString() << int(QIODevice::ReadOnly) << false
        << QFile::OpenError;

    QTest::newRow("two-dots") << m_twoDotsFile << int(QIODevice::ReadOnly) << true
        << QFile::NoError;

    QTest::newRow("readonlyfile") << QString::fromLatin1(readOnlyFile) << int(QIODevice::WriteOnly)
                                  << false << QFile::OpenError;
    QTest::newRow("noreadfile") << QString::fromLatin1(noReadFile) << int(QIODevice::ReadOnly)
                                << false << QFile::OpenError;
    QTest::newRow("resource_file") << QString::fromLatin1(":/does/not/exist")
                                   << int(QIODevice::ReadOnly)
                                   << false
                                   << QFile::OpenError;
#if defined(Q_OS_WIN)
    //opening devices requires administrative privileges (and elevation).
    HANDLE hTest = CreateFile(_T("\\\\.\\PhysicalDrive0"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hTest != INVALID_HANDLE_VALUE) {
        CloseHandle(hTest);
        QTest::newRow("//./PhysicalDrive0") << QString("//./PhysicalDrive0") << int(QIODevice::ReadOnly)
                                            << true << QFile::NoError;
    } else {
        QTest::newRow("//./PhysicalDrive0") << QString("//./PhysicalDrive0") << int(QIODevice::ReadOnly)
                                            << false << QFile::OpenError;
    }
    QTest::newRow("uncFile") << "//" + QTest::uncServerName() + "/testshare/test.pri" << int(QIODevice::ReadOnly)
                             << true << QFile::NoError;
#endif
}

void tst_QFile::open()
{
    QFETCH( QString, filename );
    QFETCH( int, mode );

    QFile f( filename );

    QFETCH( bool, ok );

#if defined(Q_OS_UNIX) && !defined(Q_OS_VXWORKS) && !defined(Q_OS_WASM)
    if (::getuid() == 0)
        // root and Chuck Norris don't care for file permissions. Skip.
        QSKIP("Running this test as root doesn't make sense");
#endif

#if defined(Q_OS_WIN32)
    QEXPECT_FAIL("noreadfile", "Windows does not currently support non-readable files.", Abort);
#endif
    if (filename.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, "QFSFileEngine::open: No file name specified");

    const QIODevice::OpenMode om(mode);
    const bool succeeded = f.open(om);
    if (ok)
        QVERIFY2(succeeded, msgOpenFailed(om, f).constData());
    else
        QVERIFY(!succeeded);

    QTEST( f.error(), "status" );
}

void tst_QFile::openUnbuffered()
{
    QFile file(m_testFile);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Unbuffered), msgOpenFailed(file).constData());
    char c = '\0';
    QVERIFY(file.seek(1));
    QCOMPARE(file.pos(), qint64(1));
    QVERIFY(file.getChar(&c));
    QCOMPARE(file.pos(), qint64(2));
    char d = '\0';
    QVERIFY(file.seek(3));
    QCOMPARE(file.pos(), qint64(3));
    QVERIFY(file.getChar(&d));
    QCOMPARE(file.pos(), qint64(4));
    QVERIFY(file.seek(1));
    QCOMPARE(file.pos(), qint64(1));
    char c2 = '\0';
    QVERIFY(file.getChar(&c2));
    QCOMPARE(file.pos(), qint64(2));
    QVERIFY(file.seek(3));
    QCOMPARE(file.pos(), qint64(3));
    char d2 = '\0';
    QVERIFY(file.getChar(&d2));
    QCOMPARE(file.pos(), qint64(4));
    QCOMPARE(c, c2);
    QCOMPARE(d, d2);
    QCOMPARE(c, '-');
    QCOMPARE(d, '-');
}

void tst_QFile::size_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<qint64>("size");

    QTest::newRow( "exist01" ) << m_testFile << (qint64)245;
#if defined(Q_OS_WIN)
    // Only test UNC on Windows./
    QTest::newRow("unc") << "//" + QString(QTest::uncServerName() + "/testshare/test.pri") << (qint64)34;
#endif
}

void tst_QFile::size()
{
    QFETCH( QString, filename );
    QFETCH( qint64, size );

    {
        QFile f( filename );
        QCOMPARE( f.size(), size );

        QVERIFY2(f.open(QIODevice::ReadOnly), msgOpenFailed(f).constData());
        QCOMPARE( f.size(), size );
    }

    {
        StdioFileGuard stream(QT_FOPEN(filename.toLocal8Bit().constData(), "rb"));
        QVERIFY( stream );
        QFile f;
        QVERIFY( f.open(stream, QIODevice::ReadOnly) );
        QCOMPARE( f.size(), size );

        f.close();
    }

    {
        QFile f;

        int fd = QT_OPEN(filename.toLocal8Bit().constData(), QT_OPEN_RDONLY);

        QVERIFY( fd != -1 );
        QVERIFY( f.open(fd, QIODevice::ReadOnly) );
        QCOMPARE( f.size(), size );

        f.close();
        QT_CLOSE(fd);
    }
}

void tst_QFile::sizeNoExist()
{
    QFile file("nonexist01");
    QVERIFY( !file.exists() );
    QCOMPARE( file.size(), (qint64)0 );
    QVERIFY( !file.open(QIODevice::ReadOnly) );
}

void tst_QFile::seek()
{
    QFile file("newfile.txt");
    QVERIFY2(file.open(QIODevice::WriteOnly), msgOpenFailed(file).constData());
    QCOMPARE(file.size(), qint64(0));
    QCOMPARE(file.pos(), qint64(0));
    QVERIFY(file.seek(10));
    QCOMPARE(file.pos(), qint64(10));
    QCOMPARE(file.size(), qint64(0));
    file.close();
}

void tst_QFile::setSize()
{
    QFile f("createme.txt");
    QVERIFY2(f.open(QIODevice::Truncate | QIODevice::ReadWrite), msgOpenFailed(f).constData());
    f.putChar('a');

    f.seek(0);
    char c = '\0';
    f.getChar(&c);
    QCOMPARE(c, 'a');

    QCOMPARE(f.size(), (qlonglong)1);
    bool ok = f.resize(99);
    QVERIFY(ok);
    QCOMPARE(f.size(), (qlonglong)99);

    f.seek(0);
    c = '\0';
    f.getChar(&c);
    QCOMPARE(c, 'a');

    QVERIFY(f.resize(1));
    QCOMPARE(f.size(), (qlonglong)1);

    f.seek(0);
    c = '\0';
    f.getChar(&c);
    QCOMPARE(c, 'a');

    f.close();

    QCOMPARE(f.size(), (qlonglong)1);
    QVERIFY(f.resize(100));
    QCOMPARE(f.size(), (qlonglong)100);
    QVERIFY(f.resize(50));
    QCOMPARE(f.size(), (qlonglong)50);
}

void tst_QFile::setSizeSeek()
{
    QFile f("setsizeseek.txt");
    QVERIFY2(f.open(QFile::WriteOnly), msgOpenFailed(f).constData());
    f.write("ABCD");

    QCOMPARE(f.pos(), qint64(4));
    f.resize(2);
    QCOMPARE(f.pos(), qint64(2));
    f.resize(4);
    QCOMPARE(f.pos(), qint64(2));
    f.resize(0);
    QCOMPARE(f.pos(), qint64(0));
    f.resize(4);
    QCOMPARE(f.pos(), qint64(0));

    f.seek(3);
    QCOMPARE(f.pos(), qint64(3));
    f.resize(2);
    QCOMPARE(f.pos(), qint64(2));
}

void tst_QFile::atEnd()
{
    QFile f( m_testFile );
    QVERIFY2(f.open(QFile::ReadOnly), msgOpenFailed(f).constData());

    int size = f.size();
    f.seek( size );

    bool end = f.atEnd();
    f.close();
    QVERIFY(end);
}

void tst_QFile::readLine()
{
    QFile f( m_testFile );
    QVERIFY2(f.open(QFile::ReadOnly), msgOpenFailed(f).constData());

    int i = 0;
    char p[128];
    int foo;
    while ( (foo=f.readLine( p, 128 )) > 0 ) {
        ++i;
        if ( i == 5 ) {
            QCOMPARE( p[0], 'T' );
            QCOMPARE( p[3], 's' );
            QCOMPARE( p[11], 'i' );
        }
    }
    f.close();
    QCOMPARE( i, 6 );
}

void tst_QFile::readLine2()
{
    QFile f( m_testFile );
    QVERIFY2(f.open(QFile::ReadOnly), msgOpenFailed(f).constData());


    char p[128];
    QCOMPARE(f.readLine(p, 60), qlonglong(59));
    QCOMPARE(f.readLine(p, 60), qlonglong(59));
    memset(p, '@', sizeof(p));
    QCOMPARE(f.readLine(p, 60), qlonglong(59));

    QCOMPARE(p[57], '-');
    QCOMPARE(p[58], '\n');
    QCOMPARE(p[59], '\0');
    QCOMPARE(p[60], '@');
}

void tst_QFile::readLineNullInLine()
{
    QFile::remove("nullinline.txt");
    QFile file("nullinline.txt");
    QVERIFY2(file.open(QFile::ReadWrite), msgOpenFailed(file).constData());
    QVERIFY(file.write("linewith\0null\nanotherline\0withnull\n\0\nnull\0", 42) > 0);
    QVERIFY(file.flush());
    file.reset();

    QCOMPARE(file.readLine(), QByteArray("linewith\0null\n", 14));
    QCOMPARE(file.readLine(), QByteArray("anotherline\0withnull\n", 21));
    QCOMPARE(file.readLine(), QByteArray("\0\n", 2));
    QCOMPARE(file.readLine(), QByteArray("null\0", 5));
    QCOMPARE(file.readLine(), QByteArray());
}

void tst_QFile::readAll_data()
{
    QTest::addColumn<bool>("textMode");
    QTest::addColumn<QString>("fileName");
    QTest::newRow( "TextMode unixfile" ) <<  true << m_testFile;
    QTest::newRow( "BinaryMode unixfile" ) <<  false << m_testFile;
    QTest::newRow( "TextMode dosfile" ) <<  true << m_dosFile;
    QTest::newRow( "BinaryMode dosfile" ) <<  false << m_dosFile;
    QTest::newRow( "TextMode bigfile" ) <<  true << m_testSourceFile;
    QTest::newRow( "BinaryMode  bigfile" ) <<  false << m_testSourceFile;
    QVERIFY(QFile(m_testSourceFile).size() > 64*1024);
}

void tst_QFile::readAll()
{
    QFETCH( bool, textMode );
    QFETCH( QString, fileName );

    QFile file(fileName);
    const QIODevice::OpenMode om = textMode ? (QFile::Text | QFile::ReadOnly) : QFile::ReadOnly;
    QVERIFY2(file.open(om), msgOpenFailed(om, file).constData());

    QByteArray a = file.readAll();
    file.reset();
    QCOMPARE(file.pos(), 0);

    QVERIFY(file.bytesAvailable() > 7);
    QByteArray b = file.read(1);
    char x;
    file.getChar(&x);
    b.append(x);
    b.append(file.read(5));
    b.append(file.readAll());

    QCOMPARE(a, b);
}

void tst_QFile::readAllBuffer()
{
    QString fileName = QLatin1String("readAllBuffer.txt");

    QFile::remove(fileName);

    QFile writer(fileName);
    QFile reader(fileName);

    QByteArray data1("This is arguably a very simple text.");
    QByteArray data2("This is surely not as simple a test.");

    QVERIFY2(writer.open(QIODevice::ReadWrite | QIODevice::Unbuffered), msgOpenFailed(writer).constData());
    QVERIFY2(reader.open(QIODevice::ReadOnly), msgOpenFailed(reader).constData());

    QCOMPARE( writer.write(data1), qint64(data1.size()) );
    QVERIFY( writer.seek(0) );

    QByteArray result;
    result = reader.read(18);
    QCOMPARE( result.size(), 18 );

    QCOMPARE( writer.write(data2), qint64(data2.size()) ); // new data, old version buffered in reader
    QCOMPARE( writer.write(data2), qint64(data2.size()) ); // new data, unbuffered in reader

    result += reader.readAll();

    QCOMPARE( result, data1 + data2 );

    QFile::remove(fileName);
}

#if QT_CONFIG(process)
class StdinReaderProcessGuard { // Ensure the stdin reader process is stopped on destruction.
    Q_DISABLE_COPY(StdinReaderProcessGuard)

public:
    StdinReaderProcessGuard(QProcess *p) : m_process(p) {}
    ~StdinReaderProcessGuard() { stop(); }

    bool stop(int msecs = 30000)
    {
        if (m_process->state() != QProcess::Running)
            return true;
        m_process->closeWriteChannel();
        if (m_process->waitForFinished(msecs))
            return m_process->exitStatus() == QProcess::NormalExit && !m_process->exitCode();
        m_process->terminate();
        if (!m_process->waitForFinished())
            m_process->kill();
        return false;
    }

private:
    QProcess *m_process;
};
#endif // QT_CONFIG(process)

void tst_QFile::readAllStdin()
{
#if !QT_CONFIG(process)
    QSKIP("No qprocess support");
#else
#if defined(Q_OS_ANDROID)
    QSKIP("This test crashes when doing nanosleep. See QTBUG-69034.");
#endif
    QByteArray lotsOfData(1024, '@'); // 10 megs

    QProcess process;
    StdinReaderProcessGuard processGuard(&process);
    process.start(m_stdinProcess, QStringList(QStringLiteral("all")));
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    for (int i = 0; i < 5; ++i) {
        process.write(lotsOfData);
        while (process.bytesToWrite() > 0)
            QVERIFY(process.waitForBytesWritten());
    }

    QVERIFY(processGuard.stop());
    QCOMPARE(process.readAll().size(), lotsOfData.size() * 5);
#endif
}

void tst_QFile::readLineStdin()
{
#if !QT_CONFIG(process)
    QSKIP("No qprocess support");
#else
#if defined(Q_OS_ANDROID)
    QSKIP("This test crashes when doing nanosleep. See QTBUG-69034.");
#endif
    QByteArray lotsOfData(1024, '@'); // 10 megs
    for (int i = 0; i < lotsOfData.size(); ++i) {
        if ((i % 32) == 31)
            lotsOfData[i] = '\n';
        else
            lotsOfData[i] = char('0' + i % 32);
    }

    for (int i = 0; i < 2; ++i) {
        QProcess process;
        StdinReaderProcessGuard processGuard(&process);
        process.start(m_stdinProcess,
                      QStringList() << QStringLiteral("line") << QString::number(i),
                      QIODevice::Text | QIODevice::ReadWrite);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        for (int i = 0; i < 5; ++i) {
            process.write(lotsOfData);
            while (process.bytesToWrite() > 0)
                QVERIFY(process.waitForBytesWritten());
        }

        QVERIFY(processGuard.stop(5000));

        QByteArray array = process.readAll();
        QCOMPARE(array.size(), lotsOfData.size() * 5);
        for (int i = 0; i < array.size(); ++i) {
            if ((i % 32) == 31)
                QCOMPARE(char(array[i]), '\n');
            else
                QCOMPARE(char(array[i]), char('0' + i % 32));
        }
    }
#endif
}

void tst_QFile::readLineStdin_lineByLine()
{
#if !QT_CONFIG(process)
    QSKIP("No qprocess support");
#else
#if defined(Q_OS_ANDROID)
    QSKIP("This test crashes when calling ::poll. See QTBUG-69034.");
#endif
    for (int i = 0; i < 2; ++i) {
        QProcess process;
        StdinReaderProcessGuard processGuard(&process);
        process.start(m_stdinProcess,
                      QStringList() << QStringLiteral("line") << QString::number(i),
                      QIODevice::Text | QIODevice::ReadWrite);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));

        for (int j = 0; j < 3; ++j) {
            QByteArray line = "line " + QByteArray::number(j) + "\n";
            QCOMPARE(process.write(line), qint64(line.size()));
            QVERIFY(process.waitForBytesWritten(2000));
            if (process.bytesAvailable() == 0)
                QVERIFY(process.waitForReadyRead(2000));
            QCOMPARE(process.readAll(), line);
        }

        QVERIFY(processGuard.stop(5000));
    }
#endif
}

void tst_QFile::text()
{
    // dosfile.txt is a binary CRLF file
    QFile file(m_dosFile);
    QVERIFY2(file.open(QFile::Text | QFile::ReadOnly), msgOpenFailed(file).constData());
    QCOMPARE(file.readLine(),
            QByteArray("/dev/system/root     /                    reiserfs   acl,user_xattr        1 1\n"));
    QCOMPARE(file.readLine(),
            QByteArray("/dev/sda1            /boot                ext3       acl,user_xattr        1 2\n"));
    file.ungetChar('\n');
    file.ungetChar('2');
    QCOMPARE(file.readLine().constData(), QByteArray("2\n").constData());
}

void tst_QFile::missingEndOfLine()
{
    QFile file(m_noEndOfLineFile);
    QVERIFY2(file.open(QFile::ReadOnly), msgOpenFailed(file).constData());

    int nlines = 0;
    while (!file.atEnd()) {
        ++nlines;
        file.readLine();
    }

    QCOMPARE(nlines, 3);
}

void tst_QFile::readBlock()
{
    QFile f( m_testFile );
    QVERIFY( f.open( QIODevice::ReadOnly ) );

    int length = 0;
    char p[256];
    length = f.read( p, 256 );
    f.close();
    QCOMPARE( length, 245 );
    QCOMPARE( p[59], 'D' );
    QCOMPARE( p[178], 'T' );
    QCOMPARE( p[199], 'l' );
}

void tst_QFile::getch()
{
    QFile f( m_testFile );
    QVERIFY( f.open( QIODevice::ReadOnly ) );

    char c;
    int i = 0;
    while (f.getChar(&c)) {
        QCOMPARE(f.pos(), qint64(i + 1));
        if ( i == 59 )
            QCOMPARE( c, 'D' );
        ++i;
    }
    f.close();
    QCOMPARE( i, 245 );
}

void tst_QFile::ungetChar()
{
    QFile f(m_testFile);
    QVERIFY2(f.open(QFile::ReadOnly), msgOpenFailed(f).constData());

    QByteArray array = f.readLine();
    QCOMPARE(array.constData(), "----------------------------------------------------------\n");
    f.ungetChar('\n');

    array = f.readLine();
    QCOMPARE(array.constData(), "\n");

    f.ungetChar('\n');
    f.ungetChar('-');
    f.ungetChar('-');

    array = f.readLine();
    QCOMPARE(array.constData(), "--\n");

    QFile::remove("genfile.txt");
    QFile out("genfile.txt");
    QVERIFY2(out.open(QIODevice::ReadWrite), msgOpenFailed(out).constData());
    out.write("123");
    out.seek(0);
    QCOMPARE(out.readAll().constData(), "123");
    out.ungetChar('3');
    out.write("4");
    out.seek(0);
    QCOMPARE(out.readAll().constData(), "124");
    out.ungetChar('4');
    out.ungetChar('2');
    out.ungetChar('1');
    char buf[3];
    QCOMPARE(out.read(buf, sizeof(buf)), qint64(3));
    QCOMPARE(buf[0], '1');
    QCOMPARE(buf[1], '2');
    QCOMPARE(buf[2], '4');
}

#if defined(Q_OS_WIN)
QString driveLetters()
{
    wchar_t volumeName[MAX_PATH];
    wchar_t path[MAX_PATH];
    const HANDLE h = FindFirstVolumeW(volumeName, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE)
        return QString();
    QString result;
    do {
        if (GetVolumePathNamesForVolumeNameW(volumeName, path, MAX_PATH, NULL)) {
            if (path[1] == L':')
                result.append(QChar(path[0]));
        }
    } while (FindNextVolumeW(h, volumeName, MAX_PATH));
    FindVolumeClose(h);
    return result;
}

static inline QChar invalidDriveLetter()
{
    const QString drives = driveLetters().toLower();
    for (char c = 'a'; c <= 'z'; ++c)
        if (!drives.contains(QLatin1Char(c)))
            return QLatin1Char(c);
    Q_ASSERT(false); // All drive letters used?!
    return QChar();
}

#endif // Q_OS_WIN

void tst_QFile::invalidFile_data()
{
    QTest::addColumn<QString>("fileName");

#if defined(Q_OS_WASM)
    QSKIP("No invalid files on wasm");
#endif

#if !defined(Q_OS_WIN)
    QTest::newRow( "x11" ) << QString( "qwe//" );
#else
    QTest::newRow( "colon2" ) << invalidDriveLetter() + QString::fromLatin1(":ail:invalid");
    QTest::newRow( "colon3" ) << QString( ":failinvalid" );
    QTest::newRow( "forwardslash" ) << QString( "fail/invalid" );
    QTest::newRow( "asterisk" ) << QString( "fail*invalid" );
    QTest::newRow( "questionmark" ) << QString( "fail?invalid" );
    QTest::newRow( "quote" ) << QString( "fail\"invalid" );
    QTest::newRow( "lt" ) << QString( "fail<invalid" );
    QTest::newRow( "gt" ) << QString( "fail>invalid" );
    QTest::newRow( "pipe" ) << QString( "fail|invalid" );
#endif
}
void tst_QFile::invalidFile()
{
    QFETCH( QString, fileName );
    QFile f( fileName );
    QVERIFY2( !f.open( QIODevice::ReadWrite ), qPrintable(fileName) );
}

void tst_QFile::createFile()
{
    if ( QFile::exists( "createme.txt" ) )
        QFile::remove( "createme.txt" );
    QVERIFY( !QFile::exists( "createme.txt" ) );

    QFile f( "createme.txt" );
    QVERIFY2( f.open(QIODevice::WriteOnly), msgOpenFailed(f).constData());
    f.close();
    QVERIFY( QFile::exists( "createme.txt" ) );
}

void tst_QFile::createFileNewOnly()
{
    QFile::remove("createme.txt");
    QVERIFY(!QFile::exists("createme.txt"));

    QFile f("createme.txt");
    QVERIFY2(f.open(QIODevice::NewOnly), msgOpenFailed(f).constData());
    f.close();
    QVERIFY(QFile::exists("createme.txt"));

    QVERIFY(!f.open(QIODevice::NewOnly));
    QVERIFY(QFile::exists("createme.txt"));
    QFile::remove("createme.txt");
}

void tst_QFile::createFilePermissions_data()
{
    QTest::addColumn<QFile::Permissions>("permissions");

    for (int u = 0; u < 8; ++u) {
        for (int g = 0; g < 8; ++g) {
            for (int o = 0; o < 8; ++o) {
                auto permissions = QFileDevice::Permissions::fromInt((u << 12) | (g << 4) | o);
                QTest::addRow("%04x", permissions.toInt()) << permissions;
            }
        }
    }
}

void tst_QFile::createFilePermissions()
{
    QFETCH(QFile::Permissions, permissions);

#ifdef Q_OS_WIN
    QNtfsPermissionCheckGuard permissionGuard;
#endif
#ifdef Q_OS_UNIX
    auto restoreMask = qScopeGuard([oldMask = umask(0)] { umask(oldMask); });
#endif

    const QFile::Permissions setPermissions = {
        QFile::ReadOther, QFile::WriteOther, QFile::ExeOther,
        QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup,
        QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner
    };

    const QString fileName = u"createme.txt"_s;

    QFile::remove(fileName);
    QVERIFY(!QFile::exists(fileName));

    QFile f(fileName);
    auto removeFile = qScopeGuard([&f] {
        f.close();
        f.remove();
    });
    QVERIFY2(f.open(QIODevice::WriteOnly, permissions), msgOpenFailed(f).constData());

    QVERIFY(QFile::exists(fileName));

    auto actualPermissions = QFileInfo(fileName).permissions();
    QCOMPARE(actualPermissions & setPermissions, permissions);
}

void tst_QFile::openFileExistingOnly()
{
    QFile::remove("dontcreateme.txt");
    QVERIFY(!QFile::exists("dontcreateme.txt"));

    QFile f("dontcreateme.txt");
    QVERIFY(!f.open(QIODevice::ExistingOnly | QIODevice::ReadOnly));
    QVERIFY(!f.open(QIODevice::ExistingOnly | QIODevice::WriteOnly));
    QVERIFY(!f.open(QIODevice::ExistingOnly | QIODevice::ReadWrite));
    QVERIFY(!f.open(QIODevice::ExistingOnly));
    QVERIFY(!QFile::exists("dontcreateme.txt"));

    QVERIFY2(f.open(QIODevice::NewOnly), msgOpenFailed(f).constData());
    f.close();
    QVERIFY(QFile::exists("dontcreateme.txt"));

    QVERIFY2(f.open(QIODevice::ExistingOnly | QIODevice::ReadOnly), msgOpenFailed(f).constData());
    f.close();
    QVERIFY2(f.open(QIODevice::ExistingOnly | QIODevice::WriteOnly), msgOpenFailed(f).constData());
    f.close();
    QVERIFY2(f.open(QIODevice::ExistingOnly | QIODevice::ReadWrite), msgOpenFailed(f).constData());
    f.close();
    QVERIFY(!f.open(QIODevice::ExistingOnly));
    QVERIFY(QFile::exists("dontcreateme.txt"));
    QFile::remove("dontcreateme.txt");
}

void tst_QFile::append()
{
    const QString name("appendme.txt");
    if (QFile::exists(name))
        QFile::remove(name);
    QVERIFY(!QFile::exists(name));

    QFile f(name);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate), msgOpenFailed(f).constData());
    f.putChar('a');
    f.close();

    QVERIFY2(f.open(QIODevice::Append), msgOpenFailed(f).constData());
    QCOMPARE(f.pos(), 1);
    f.putChar('a');
    f.close();
    QCOMPARE(int(f.size()), 2);

    QVERIFY2(f.open(QIODevice::Append | QIODevice::Truncate), msgOpenFailed(f).constData());
    QCOMPARE(f.pos(), 0);
    f.putChar('a');
    f.close();
    QCOMPARE(int(f.size()), 1);
}

void tst_QFile::permissions_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<uint>("perms");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<bool>("create");

#ifndef Q_OS_WASM
    // Application path is empty on wasm
    QTest::newRow("data0") << QCoreApplication::instance()->applicationFilePath() << uint(QFile::ExeUser) << true << false;
#endif
    QTest::newRow("data1") << m_testSourceFile << uint(QFile::ReadUser) << true << false;
    QTest::newRow("readonly") << QString::fromLatin1("readonlyfile") << uint(QFile::WriteUser) << false << false;
    QTest::newRow("longfile") << QString::fromLatin1("longFileNamelongFileNamelongFileNamelongFileName"
                                                    "longFileNamelongFileNamelongFileNamelongFileName"
                                                    "longFileNamelongFileNamelongFileNamelongFileName"
                                                    "longFileNamelongFileNamelongFileNamelongFileName"
                                                    "longFileNamelongFileNamelongFileNamelongFileName.txt") << uint(QFile::ReadUser) << true << true;
    QTest::newRow("resource1") << ":/tst_qfile/resources/file1.ext1" << uint(QFile::ReadUser) << true << false;
    QTest::newRow("resource2") << ":/tst_qfile/resources/file1.ext1" << uint(QFile::WriteUser) << false << false;
    QTest::newRow("resource3") << ":/tst_qfile/resources/file1.ext1" << uint(QFile::ExeUser) << false << false;
}

void tst_QFile::permissions()
{
    QFETCH(QString, file);
    QFETCH(uint, perms);
    QFETCH(bool, expected);
    QFETCH(bool, create);
    if (create) {
        QFile fc(file);
        QVERIFY2(fc.open(QFile::WriteOnly), msgOpenFailed(fc).constData());
        QVERIFY(fc.write("hello\n"));
        fc.close();
    }

    QFile f(file);
    QFile::Permissions memberResult = f.permissions() & perms;
    QFile::Permissions staticResult = QFile::permissions(file) & perms;

    if (create) {
        QVERIFY(QFile::remove(file));
    }

#if defined(Q_OS_WIN)
    if (qAreNtfsPermissionChecksEnabled())
        QEXPECT_FAIL("readonly", "QTBUG-25630", Abort);
#endif
#ifdef Q_OS_UNIX
    if (strcmp(QTest::currentDataTag(), "readonly") == 0) {
        // in case accidentally run as root
        if (::getuid() == 0)
            QSKIP("Running this test as root doesn't make sense");
    }
#endif
    QCOMPARE((memberResult == QFile::Permissions(perms)), expected);
    QCOMPARE((staticResult == QFile::Permissions(perms)), expected);
}

#ifdef Q_OS_WIN
void tst_QFile::permissionsNtfs_data()
{
    permissions_data();
}

void tst_QFile::permissionsNtfs()
{
    QNtfsPermissionCheckGuard permissionGuard;
    permissions();
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#if QT_DEPRECATED_SINCE(6,6)
void tst_QFile::deprecatedNtfsPermissionCheck()
{
    QScopedValueRollback<int> guard(qt_ntfs_permission_lookup);

    QCOMPARE(qAreNtfsPermissionChecksEnabled(), false);
    qt_ntfs_permission_lookup++;
    QCOMPARE(qAreNtfsPermissionChecksEnabled(), true);
    qt_ntfs_permission_lookup--;
    QCOMPARE(qAreNtfsPermissionChecksEnabled(), false);
}
#endif
QT_WARNING_POP

#endif

void tst_QFile::setPermissions_data()
{
    QTest::addColumn<bool>("opened");
    QTest::newRow("closed") << false;       // chmod()
    QTest::newRow("opened") << true;        // fchmod()
}

void tst_QFile::setPermissions()
{
#ifdef Q_OS_UNIX
    if (::getuid() == 0)
        QSKIP("Running this test as root doesn't make sense");
#endif
    QFETCH(bool, opened);

    auto remove = []() { QFile::remove("createme.txt"); };
    auto guard = qScopeGuard(remove);
    remove();
    QVERIFY( !QFile::exists( "createme.txt" ) );

    QFile f("createme.txt");
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate), msgOpenFailed(f).constData());
    f.putChar('a');
    if (!opened)
        f.close();

    QFile::Permissions perms(QFile::WriteUser | QFile::ReadUser);
    QVERIFY(f.setPermissions(QFile::ReadUser));
    QVERIFY((f.permissions() & perms) == QFile::ReadUser);
    QVERIFY(f.setPermissions(perms));
    QVERIFY((f.permissions() & perms) == perms);

    // we should end the test with the file in writeable state
}

void tst_QFile::copy()
{
    QFile::setPermissions("tst_qfile_copy.cpp", QFile::WriteUser);
    QFile::remove("tst_qfile_copy.cpp");
    QFile::remove("test2");
    QVERIFY(QFile::copy(m_testSourceFile, "tst_qfile_copy.cpp"));
    QFile in1(m_testSourceFile), in2("tst_qfile_copy.cpp");
    QVERIFY2(in1.open(QFile::ReadOnly), msgOpenFailed(in1).constData());
    QVERIFY2(in2.open(QFile::ReadOnly), msgOpenFailed(in2).constData());
    QByteArray data1 = in1.readAll(), data2 = in2.readAll();
    QCOMPARE(data1, data2);
    QFile::remove( "main_copy.cpp" );

    QFile::copy(QDir::currentPath(), QDir::currentPath() + QLatin1String("/test2"));
}

void tst_QFile::copyAfterFail()
{
    QFile file1("file-to-be-copied.txt");
    QFile file2("existing-file.txt");

    QVERIFY2(file1.open(QIODevice::ReadWrite), msgOpenFailed(file1).constData());
    QVERIFY2(file2.open(QIODevice::ReadWrite), msgOpenFailed(file1).constData());
    file2.close();
    QVERIFY(!QFile::exists("copied-file-1.txt") && "(test-precondition)");
    QVERIFY(!QFile::exists("copied-file-2.txt") && "(test-precondition)");

    QVERIFY(!file1.copy("existing-file.txt"));
    QCOMPARE(file1.error(), QFile::CopyError);

    QVERIFY(file1.copy("copied-file-1.txt"));
    QVERIFY(!file1.isOpen());
    QCOMPARE(file1.error(), QFile::NoError);

    QVERIFY(!file1.copy("existing-file.txt"));
    QCOMPARE(file1.error(), QFile::CopyError);

    QVERIFY(file1.copy("copied-file-2.txt"));
    QVERIFY(!file1.isOpen());
    QCOMPARE(file1.error(), QFile::NoError);

    QVERIFY(QFile::exists("copied-file-1.txt"));
    QVERIFY(QFile::exists("copied-file-2.txt"));
}

void tst_QFile::copyRemovesTemporaryFile() const
{
    const QString newName(QLatin1String("copyRemovesTemporaryFile"));
    QVERIFY(QFile::copy(m_forCopyingFile, newName));

    QVERIFY(!QFile::exists(QStringLiteral("qt_temp.XXXXXX")));
}

void tst_QFile::copyShouldntOverwrite()
{
    // Copy should not overwrite existing files.
    QFile::remove("tst_qfile.cpy");
    QFile file(m_testSourceFile);
    QVERIFY(file.copy("tst_qfile.cpy"));

    bool ok = QFile::setPermissions("tst_qfile.cpy", QFile::WriteOther);
    QVERIFY(ok);
    QVERIFY(!file.copy("tst_qfile.cpy"));
}

void tst_QFile::copyFallback()
{
    // Using a resource file to trigger QFile::copy's fallback handling
    QFile file(":/copy-fallback.qrc");
    QFile::remove("file-copy-destination.txt");

    QVERIFY2(file.exists(), "test precondition");
    QVERIFY2(!QFile::exists("file-copy-destination.txt"), "test precondition");

    // Fallback copy of closed file.
    QVERIFY(file.copy("file-copy-destination.txt"));
    QVERIFY(QFile::exists("file-copy-destination.txt"));
    QVERIFY(!file.isOpen());

     // Need to reset permissions on Windows to be able to delete
    QVERIFY(QFile::setPermissions("file-copy-destination.txt",
           QFile::ReadOwner | QFile::WriteOwner));
    QVERIFY(QFile::remove("file-copy-destination.txt"));

    // Fallback copy of open file.
    QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());
    QVERIFY(file.copy("file-copy-destination.txt"));
    QVERIFY(QFile::exists("file-copy-destination.txt"));
    QVERIFY(!file.isOpen());

    file.close();
    QFile::setPermissions("file-copy-destination.txt",
            QFile::ReadOwner | QFile::WriteOwner);
}

#ifdef Q_OS_WIN
#include <objbase.h>
#include <shlobj.h>
#endif

#if defined(Q_OS_WIN)
static QString getWorkingDirectoryForLink(const QString &linkFileName)
{
    QString ret;

    QComHelper comHelper;
    IShellLink *psl;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);

    if (SUCCEEDED(hres)) {    // Get pointer to the IPersistFile interface.
        IPersistFile *ppf;
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID *)&ppf);
        if (SUCCEEDED(hres))  {
            hres = ppf->Load((LPOLESTR)linkFileName.utf16(), STGM_READ);
            //The original path of the link is retrieved. If the file/folder
            //was moved, the return value still have the old path.
            if(SUCCEEDED(hres)) {
                wchar_t szGotPath[MAX_PATH];
                if (psl->GetWorkingDirectory(szGotPath, MAX_PATH) == NOERROR)
                    ret = QString::fromWCharArray(szGotPath);
            }
            ppf->Release();
        }
        psl->Release();
    }

    return ret;
}
#endif

void tst_QFile::link()
{
    QFile::remove("myLink.lnk");

    QFileInfo info1(m_testSourceFile);
    QString referenceTarget = QDir::cleanPath(info1.absoluteFilePath());

    QVERIFY(QFile::link(m_testSourceFile, "myLink.lnk"));

    QFileInfo info2("myLink.lnk");
    QVERIFY(info2.isSymLink());
    QCOMPARE(info2.symLinkTarget(), referenceTarget);

    QFile link("myLink.lnk");
    QVERIFY2(link.open(QIODevice::ReadOnly), msgOpenFailed(link).constData());
    QCOMPARE(link.symLinkTarget(), referenceTarget);
    link.close();

    QCOMPARE(QFile::symLinkTarget("myLink.lnk"), referenceTarget);

#if defined(Q_OS_WIN)
    QString wd = getWorkingDirectoryForLink(info2.absoluteFilePath());
    QCOMPARE(QDir::fromNativeSeparators(wd), QDir::cleanPath(info1.absolutePath()));
#endif
}

void tst_QFile::linkToDir()
{
    QFile::remove("myLinkToDir.lnk");
    QDir dir;
    dir.mkdir("myDir");
    QFileInfo info1("myDir");
    QVERIFY(QFile::link("myDir", "myLinkToDir.lnk"));
    QFileInfo info2("myLinkToDir.lnk");
#if !(defined Q_OS_HPUX && defined(__ia64))
    // absurd HP-UX filesystem bug on gravlaks - checking if a symlink
    // resolves or not alters the file system to make the broken symlink
    // later fail...
    QVERIFY(info2.isSymLink());
#endif
    QCOMPARE(info2.symLinkTarget(), info1.absoluteFilePath());
    QVERIFY(QFile::remove(info2.absoluteFilePath()));
}

void tst_QFile::absolutePathLinkToRelativePath()
{
    QFile::remove("myDir/test.txt");
    QFile::remove("myDir/myLink.lnk");
    QDir dir;
    dir.mkdir("myDir");
    QVERIFY(QFile("myDir/test.txt").open(QFile::WriteOnly));

#ifdef Q_OS_WIN
    QVERIFY(QFile::link("test.txt", "myDir/myLink.lnk"));
#else
    QVERIFY(QFile::link("myDir/test.txt", "myDir/myLink.lnk"));
#endif
    QEXPECT_FAIL("", "Symlinking using relative paths is currently different on Windows and Unix", Continue);
    QCOMPARE(QFileInfo(QFile(QFileInfo("myDir/myLink.lnk").absoluteFilePath()).symLinkTarget()).absoluteFilePath(),
             QFileInfo("myDir/test.txt").absoluteFilePath());
}

void tst_QFile::readBrokenLink()
{
    QFile::remove("myLink2.lnk");
    QFileInfo info1("file12");
    QVERIFY(QFile::link("file12", "myLink2.lnk"));
    QFileInfo info2("myLink2.lnk");
    QVERIFY(info2.isSymLink());
    QCOMPARE(info2.symLinkTarget(), info1.absoluteFilePath());
    QVERIFY(QFile::remove(info2.absoluteFilePath()));
    QVERIFY(QFile::link("ole/..", "myLink2.lnk"));
    QCOMPARE(QFileInfo("myLink2.lnk").symLinkTarget(), QDir::currentPath());
}

void tst_QFile::readTextFile_data()
{
    QTest::addColumn<QByteArray>("in");
    QTest::addColumn<QByteArray>("out");

    QTest::newRow("empty") << QByteArray() << QByteArray();
    QTest::newRow("a") << QByteArray("a") << QByteArray("a");
    QTest::newRow("a\\rb") << QByteArray("a\rb") << QByteArray("ab");
    QTest::newRow("\\n") << QByteArray("\n") << QByteArray("\n");
    QTest::newRow("\\r\\n") << QByteArray("\r\n") << QByteArray("\n");
    QTest::newRow("\\r") << QByteArray("\r") << QByteArray();
    QTest::newRow("twolines") << QByteArray("Hello\r\nWorld\r\n") << QByteArray("Hello\nWorld\n");
    QTest::newRow("twolines no endline") << QByteArray("Hello\r\nWorld") << QByteArray("Hello\nWorld");
}

void tst_QFile::readTextFile()
{
    QFETCH(QByteArray, in);
    QFETCH(QByteArray, out);

    QFile winfile("winfile.txt");
    QVERIFY2(winfile.open(QFile::WriteOnly | QFile::Truncate), msgOpenFailed(winfile).constData());
    winfile.write(in);
    winfile.close();

    QVERIFY2(winfile.open(QFile::ReadOnly), msgOpenFailed(winfile).constData());
    QCOMPARE(winfile.readAll(), in);
    winfile.close();

    QVERIFY2(winfile.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(winfile).constData());
    QCOMPARE(winfile.readAll(), out);
}

void tst_QFile::readTextFile2()
{
    {
        QFile file(m_testLogFile);
        QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());
        file.read(4097);
    }

    {
        QFile file(m_testLogFile);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), msgOpenFailed(file).constData());
        file.read(4097);
    }
}

void tst_QFile::writeTextFile_data()
{
    QTest::addColumn<QByteArray>("in");

    QTest::newRow("empty") << QByteArray();
    QTest::newRow("a") << QByteArray("a");
    QTest::newRow("a\\rb") << QByteArray("a\rb");
    QTest::newRow("\\n") << QByteArray("\n");
    QTest::newRow("\\r\\n") << QByteArray("\r\n");
    QTest::newRow("\\r") << QByteArray("\r");
    QTest::newRow("twolines crlf") << QByteArray("Hello\r\nWorld\r\n");
    QTest::newRow("twolines crlf no endline") << QByteArray("Hello\r\nWorld");
    QTest::newRow("twolines lf") << QByteArray("Hello\nWorld\n");
    QTest::newRow("twolines lf no endline") << QByteArray("Hello\nWorld");
    QTest::newRow("mixed") << QByteArray("this\nis\r\na\nmixed\r\nfile\n");
}

void tst_QFile::writeTextFile()
{
    QFETCH(QByteArray, in);

    QFile file("textfile.txt");
    QVERIFY2(file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text),
             msgOpenFailed(file).constData());
    QByteArray out = in;
#ifdef Q_OS_WIN
    out.replace('\n', "\r\n");
#endif
    QCOMPARE(file.write(in), qlonglong(in.size()));
    file.close();

    QVERIFY(file.open(QFile::ReadOnly));
    QCOMPARE(file.readAll(), out);
}

#if defined(Q_OS_WIN)
// Helper for executing QFile::open() with warning in QTRY_VERIFY(), which evaluates the condition
// multiple times
static bool qFileOpen(QFile &file, QIODevice::OpenMode ioFlags)
{
    const bool result = file.isOpen() || file.open(ioFlags);
    if (!result)
        qWarning() << "Cannot open" << file.fileName() << ':' << file.errorString();
    return result;
}

// Helper for executing fopen() with warning in QTRY_VERIFY(), which evaluates the condition
// multiple times
static bool fOpen(const QByteArray &fileName, const char *mode, FILE **file)
{
    if (*file == nullptr)
        *file = fopen(fileName.constData(), mode);
    if (*file == nullptr)
        qWarning("Cannot open %s: %s", fileName.constData(), strerror(errno));
    return *file != nullptr;
}

void tst_QFile::largeUncFileSupport()
{
    // Currently there is a single network test server that is used by all VMs running tests in
    // the CI. This test accesses a file shared with Samba on that server. Unfortunately many
    // clients accessing the file at the same time is a sharing violation. This test already
    // attempted to deal with the problem with retries, but that has led to the test timing out,
    // not eventually succeeding. Due to the timeouts blacklisting the test wouldn't help.
    // See https://bugreports.qt.io/browse/QTQAINFRA-1727 which will be resolved by the new
    // test server architecture where the server is no longer shared.
    QSKIP("Multiple instances of running this test at the same time fail due to QTQAINFRA-1727");

    qint64 size = Q_INT64_C(8589934592);
    qint64 dataOffset = Q_INT64_C(8589914592);
    QByteArray knownData("LargeFile content at offset 8589914592");
    QString largeFile("//" + QTest::uncServerName() + "/testsharelargefile/file.bin");
    const QByteArray largeFileEncoded = QFile::encodeName(largeFile);

    {
        // 1) Native file handling.
        QFile file(largeFile);
        QVERIFY2(file.exists(), msgFileDoesNotExist(largeFile));

        QCOMPARE(file.size(), size);
        // Retry in case of sharing violation
        QTRY_VERIFY2(qFileOpen(file, QIODevice::ReadOnly), msgOpenFailed(file).constData());
        QCOMPARE(file.size(), size);
        QVERIFY(file.seek(dataOffset));
        QCOMPARE(file.read(knownData.size()), knownData);
    }
    {
        // 2) stdlib file handling.
        FILE *fhF = nullptr;
        // Retry in case of sharing violation
        QTRY_VERIFY(fOpen(largeFileEncoded, "rb", &fhF));
        StdioFileGuard fh(fhF);
        QFile file;
        QVERIFY(file.open(fh, QIODevice::ReadOnly));
        QCOMPARE(file.size(), size);
        QVERIFY(file.seek(dataOffset));
        QCOMPARE(file.read(knownData.size()), knownData);
    }
    {
        // 3) stdio file handling.
        FILE *fhF = nullptr;
        // Retry in case of sharing violation
        QTRY_VERIFY(fOpen(largeFileEncoded, "rb", &fhF));
        StdioFileGuard fh(fhF);
        int fd = int(QT_FILENO(fh));
        QFile file;
        QVERIFY(file.open(fd, QIODevice::ReadOnly));
        QCOMPARE(file.size(), size);
        QVERIFY(file.seek(dataOffset));
        QCOMPARE(file.read(knownData.size()), knownData);
    }
}
#endif

void tst_QFile::flush()
{
    QString fileName("stdfile.txt");

    QFile::remove(fileName);

    {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
        QCOMPARE(file.write("abc", 3),qint64(3));
    }

    {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::WriteOnly | QFile::Append), msgOpenFailed(file).constData());
        QCOMPARE(file.pos(), qlonglong(3));
        QCOMPARE(file.write("def", 3), qlonglong(3));
        QCOMPARE(file.pos(), qlonglong(6));
    }

    {
        QFile file("stdfile.txt");
        QVERIFY2(file.open(QFile::ReadOnly), msgOpenFailed(file).constData());
        QCOMPARE(file.readAll(), QByteArray("abcdef"));
    }
}

void tst_QFile::bufferedRead()
{
    QFile::remove("stdfile.txt");

    QFile file("stdfile.txt");
    QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
    file.write("abcdef");
    file.close();

    StdioFileGuard stdFile(fopen("stdfile.txt", "r"));
    QVERIFY(stdFile);
    char c;
    QCOMPARE(int(fread(&c, 1, 1, stdFile)), 1);
    QCOMPARE(c, 'a');
    QCOMPARE(int(ftell(stdFile)), 1);

    {
        QFile file;
        QVERIFY2(file.open(stdFile, QFile::ReadOnly), msgOpenFailed(file).constData());
        QCOMPARE(file.pos(), qlonglong(1));
        QCOMPARE(file.read(&c, 1), qlonglong(1));
        QCOMPARE(c, 'b');
        QCOMPARE(file.pos(), qlonglong(2));
    }
}

#ifdef Q_OS_UNIX
void tst_QFile::isSequential_data()
{
    QTest::addColumn<QString>("deviceName");
    QTest::addColumn<bool>("acceptFailOpen");

    QTest::newRow("/dev/null") << QString("/dev/null") << false;
    QTest::newRow("/dev/tty")  << QString("/dev/tty")  << true;
    QTest::newRow("/dev/zero") << QString("/dev/zero") << false;
}

void tst_QFile::isSequential()
{
    QFETCH(QString, deviceName);
    QFETCH(bool, acceptFailOpen);

    if (access(deviceName.toUtf8().data(), R_OK) == 0) {
        QFile device(deviceName);
        QVERIFY2(device.open(QFile::ReadOnly) || acceptFailOpen, msgOpenFailed(device).constData());
        QVERIFY(!device.isOpen() || device.isSequential());
    }
}
#endif

void tst_QFile::decodeName_data()
{
    QTest::addColumn<QByteArray>("bytearray");
    QTest::addColumn<QString>("qstring");

    QTest::newRow("null") << QByteArray() << QString();
    QTest::newRow("simple") << "/path/to/file"_ba << u"/path/to/file"_s;

#ifndef Q_OS_WIN
#  ifdef Q_OS_DARWIN
    // Mac always expects filenames in UTF-8... and decomposed...
    QTest::newRow("filé") << "/path/to/file\xCC\x81"_ba << u"/path/to/filé"_s;
#  else
    QTest::newRow("filé") << "/path/to/fil\xC3\xA9"_ba << u"/path/to/filé"_s;
#  endif
    QTest::newRow("fraction-slash")
            << "/path\342\201\204to\342\201\204file"_ba << u"/path⁄to⁄file"_s;
    QTest::newRow("fraction-slash-u16") << "/path\u2044to\u2044file"_ba << u"/path⁄to⁄file"_s;
#endif // !Q_OS_WIN
}

void tst_QFile::decodeName()
{
    QFETCH(QByteArray, bytearray);
    QFETCH(QString, qstring);

    QCOMPARE(QFile::decodeName(bytearray), qstring);
}

void tst_QFile::encodeName()
{
    QFETCH(QString, qstring);
    QFETCH(QByteArray, bytearray);

    QCOMPARE(QFile::encodeName(qstring), bytearray);
}

void tst_QFile::truncate()
{
    const QIODevice::OpenModeFlag modes[] = { QFile::ReadWrite, QIODevice::WriteOnly, QIODevice::Append };
    for (auto mode : modes) {
        QFile file("truncate.txt");
        QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
        file.write(QByteArray(200, '@'));
        file.close();

        QVERIFY2(file.open(mode | QFile::Truncate), msgOpenFailed(file).constData());
        file.write(QByteArray(100, '$'));
        file.close();

        QVERIFY2(file.open(QFile::ReadOnly), msgOpenFailed(file).constData());
        QCOMPARE(file.readAll(), QByteArray(100, '$'));
    }
}

void tst_QFile::seekToPos()
{
    {
        QFile file("seekToPos.txt");
        QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
        file.write("a\r\nb\r\nc\r\n");
        file.flush();
    }

    QFile file("seekToPos.txt");
    QVERIFY2(file.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(file).constData());
    file.seek(1);
    char c;
    QVERIFY(file.getChar(&c));
    QCOMPARE(c, '\n');

    QCOMPARE(file.pos(), qint64(3));
    file.seek(file.pos());
    QCOMPARE(file.pos(), qint64(3));

    file.seek(1);
    file.seek(file.pos());
    QCOMPARE(file.pos(), qint64(1));

}

void tst_QFile::seekAfterEndOfFile()
{
    QLatin1String filename("seekAfterEof.dat");
    QFile::remove(filename);
    {
        QFile file(filename);
        QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
        file.write("abcd");
        QCOMPARE(file.size(), qint64(4));
        file.seek(8);
        file.write("ijkl");
        QCOMPARE(file.size(), qint64(12));
        file.seek(4);
        file.write("efgh");
        QCOMPARE(file.size(), qint64(12));
        file.seek(16);
        file.write("----");
        QCOMPARE(file.size(), qint64(20));
        file.flush();
    }

    QFile file(filename);
    QVERIFY2(file.open(QFile::ReadOnly), msgOpenFailed(file).constData());
    QByteArray contents = file.readAll();
    QCOMPARE(contents.left(12), QByteArray("abcdefghijkl", 12));
    //bytes 12-15 are uninitialised so we don't care what they read as.
    QCOMPARE(contents.mid(16), QByteArray("----", 4));
    file.close();
}

void tst_QFile::FILEReadWrite()
{
    // Tests modifying a file. First creates it then reads in 4 bytes and then overwrites these
    // 4 bytes with new values. At the end check to see the file contains the new values.

    QFile::remove("FILEReadWrite.txt");

    // create test file
    {
        QFile f("FILEReadWrite.txt");
        QVERIFY2(f.open(QFile::WriteOnly), msgOpenFailed(f).constData());
        QDataStream ds(&f);
        qint8 c = 0;
        ds << c;
        c = 1;
        ds << c;
        c = 2;
        ds << c;
        c = 3;
        ds << c;
        c = 4;
        ds << c;
        c = 5;
        ds << c;
        c = 6;
        ds << c;
        c = 7;
        ds << c;
        c = 8;
        ds << c;
        c = 9;
        ds << c;
        c = 10;
        ds << c;
        c = 11;
        ds << c;
        f.close();
    }

    StdioFileGuard fp(fopen("FILEReadWrite.txt", "r+b"));
    QVERIFY(fp);
    QFile file;
    QVERIFY2(file.open(fp, QFile::ReadWrite), msgOpenFailed(file).constData());
    QDataStream sfile(&file) ;

    qint8 var1,var2,var3,var4;
    while (!sfile.atEnd())
    {
        qint64 base = file.pos();

        QCOMPARE(file.pos(), base + 0);
        sfile >> var1;
        QCOMPARE(file.pos(), base + 1);
        file.flush(); // flushing should not change the base
        QCOMPARE(file.pos(), base + 1);
        sfile >> var2;
        QCOMPARE(file.pos(), base + 2);
        sfile >> var3;
        QCOMPARE(file.pos(), base + 3);
        sfile >> var4;
        QCOMPARE(file.pos(), base + 4);
        file.seek(file.pos() - 4) ;   // Move it back 4, for we are going to write new values based on old ones
        QCOMPARE(file.pos(), base + 0);
        sfile << qint8(var1 + 5);
        QCOMPARE(file.pos(), base + 1);
        sfile << qint8(var2 + 5);
        QCOMPARE(file.pos(), base + 2);
        sfile << qint8(var3 + 5);
        QCOMPARE(file.pos(), base + 3);
        sfile << qint8(var4 + 5);
        QCOMPARE(file.pos(), base + 4);

    }
    file.close();
    fp.close();

    // check modified file
    {
        QFile f("FILEReadWrite.txt");
        QVERIFY2(f.open(QFile::ReadOnly), msgOpenFailed(file).constData());
        QDataStream ds(&f);
        qint8 c = 0;
        ds >> c;
        QCOMPARE(c, (qint8)5);
        ds >> c;
        QCOMPARE(c, (qint8)6);
        ds >> c;
        QCOMPARE(c, (qint8)7);
        ds >> c;
        QCOMPARE(c, (qint8)8);
        ds >> c;
        QCOMPARE(c, (qint8)9);
        ds >> c;
        QCOMPARE(c, (qint8)10);
        ds >> c;
        QCOMPARE(c, (qint8)11);
        ds >> c;
        QCOMPARE(c, (qint8)12);
        ds >> c;
        QCOMPARE(c, (qint8)13);
        ds >> c;
        QCOMPARE(c, (qint8)14);
        ds >> c;
        QCOMPARE(c, (qint8)15);
        ds >> c;
        QCOMPARE(c, (qint8)16);
        f.close();
    }
}


/*
#include <qglobal.h>
#define BUFFSIZE 1
#define FILESIZE   0x10000000f
void tst_QFile::largeFileSupport()
{
#ifdef Q_OS_SOLARIS
    QSKIP("Solaris does not support statfs");
#else
    qlonglong sizeNeeded = 2147483647;
    sizeNeeded *= 2;
    sizeNeeded += 1024;
    qlonglong freespace = qlonglong(0);
#ifdef Q_OS_WIN
    _ULARGE_INTEGER free;
    if (::GetDiskFreeSpaceEx((wchar_t*)QDir::currentPath().utf16(), &free, 0, 0))
        freespace = free.QuadPart;
    if (freespace != 0) {
#else
    struct statfs info;
    if (statfs(const_cast<char *>(QDir::currentPath().toLocal8Bit().constData()), &info) == 0) {
        freespace = qlonglong(info.f_bavail * info.f_bsize);
#endif
        if (freespace > sizeNeeded) {
            QFile bigFile("bigfile");
            if (bigFile.open(QFile::ReadWrite)) {
                char c[BUFFSIZE] = {'a'};
                QVERIFY(bigFile.write(c, BUFFSIZE) == BUFFSIZE);
                qlonglong oldPos = bigFile.pos();
                QVERIFY(bigFile.resize(sizeNeeded));
                QCOMPARE(oldPos, bigFile.pos());
                QVERIFY(bigFile.seek(sizeNeeded - BUFFSIZE));
                QVERIFY(bigFile.write(c, BUFFSIZE) == BUFFSIZE);

                bigFile.close();
                if (bigFile.open(QFile::ReadOnly)) {
                    QVERIFY(bigFile.read(c, BUFFSIZE) == BUFFSIZE);
                    int i = 0;
                    for (i=0; i<BUFFSIZE; i++)
                        QCOMPARE(c[i], 'a');
                    QVERIFY(bigFile.seek(sizeNeeded - BUFFSIZE));
                    QVERIFY(bigFile.read(c, BUFFSIZE) == BUFFSIZE);
                    for (i=0; i<BUFFSIZE; i++)
                        QCOMPARE(c[i], 'a');
                    bigFile.close();
                    QVERIFY(bigFile.remove());
                } else {
                    QVERIFY(bigFile.remove());
                    QFAIL("Could not reopen file");
                }
            } else {
                QFAIL("Could not open file");
            }
        } else {
            QSKIP("Not enough space to run test");
        }
    } else {
        QFAIL("Could not determin disk space");
    }
#endif
}
*/

void tst_QFile::i18nFileName_data()
{
    QTest::addColumn<QString>("fileName");

    QTest::newRow( "01" ) << QString::fromUtf8("xxxxxxx.txt");
}

void tst_QFile::i18nFileName()
{
     QFETCH(QString, fileName);
     if (QFile::exists(fileName)) {
         QVERIFY(QFile::remove(fileName));
     }
     {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::WriteOnly | QFile::Text), msgOpenFailed(file).constData());
        file.write(fileName.toUtf8());
     }
     {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(file).constData());
        QString line = QString::fromUtf8(file.readAll());
        QCOMPARE(line, fileName);
     }
}


void tst_QFile::longFileName_data()
{
    QTest::addColumn<QString>("fileName");

    QTest::newRow( "16 chars" ) << QString::fromLatin1("longFileName.txt");
    QTest::newRow( "52 chars" ) << QString::fromLatin1("longFileNamelongFileNamelongFileNamelongFileName.txt");
    QTest::newRow( "148 chars" ) << QString::fromLatin1("longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName.txt");
    QTest::newRow( "244 chars" ) << QString::fromLatin1("longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName.txt");
    QTest::newRow( "244 chars to absolutepath" ) << QFileInfo(QString::fromLatin1("longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName.txt")).absoluteFilePath();
  /* needs to be put on a windows 2000 > test machine
  QTest::newRow( "244 chars on UNC" ) <<  QString::fromLatin1("//arsia/D/troll/tmp/longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName"
                                                     "longFileNamelongFileNamelongFileNamelongFileName.txt");*/
}

void tst_QFile::longFileName()
{
    QFETCH(QString, fileName);
    if (QFile::exists(fileName)) {
        QVERIFY(QFile::remove(fileName));
    }
    {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::WriteOnly | QFile::Text), msgOpenFailed(file).constData());
        file.write(fileName.toUtf8());
    }
    {
        QFile file(fileName);
        QVERIFY2(file.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(file).constData());
        QString line = QString::fromUtf8(file.readAll());
    }
    QString newName = fileName + QLatin1Char('1');
    {
        QVERIFY(QFile::copy(fileName, newName));
        QFile file(newName);
        QVERIFY2(file.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(file).constData());
        QString line = QString::fromUtf8(file.readAll());
        QCOMPARE(line, fileName);

    }
    QVERIFY(QFile::remove(newName));
    {
        QVERIFY(QFile::rename(fileName, newName));
        QFile file(newName);
        QVERIFY2(file.open(QFile::ReadOnly | QFile::Text), msgOpenFailed(file).constData());
        QString line = QString::fromUtf8(file.readAll());
        QCOMPARE(line, fileName);
    }
    QVERIFY2(QFile::exists(newName), msgFileDoesNotExist(newName).constData());
}

#ifdef QT_BUILD_INTERNAL
class MyEngine : public QAbstractFileEngine
{
public:
    MyEngine(int n) { number = n; }

    qint64 size() const override { return 123 + number; }
    QStringList entryList(QDirListing::IteratorFlags, const QStringList &) const override
    { return QStringList(); }
    QString fileName(FileName) const override { return name; }

private:
    int number;
    QString name;
};

class MyHandler : public QAbstractFileEngineHandler
{
    Q_DISABLE_COPY_MOVE(MyHandler)
public:
    MyHandler() = default;
    std::unique_ptr<QAbstractFileEngine> create(const QString &) const override
    {
        return std::make_unique<MyEngine>(1);
    }
};

class MyHandler2 : public QAbstractFileEngineHandler
{
    Q_DISABLE_COPY_MOVE(MyHandler2)
public:
    MyHandler2() = default;

    std::unique_ptr<QAbstractFileEngine> create(const QString &) const override
    {
        return std::make_unique<MyEngine>(2);
    }
};
#endif

void tst_QFile::fileEngineHandler()
{
    // A file that does not exist has a size of 0.
    QFile::remove("ole.bull");
    QFile file("ole.bull");
    QCOMPARE(file.size(), qint64(0));

#ifdef QT_BUILD_INTERNAL
    // Instantiating our handler will enable the new engine.
    MyHandler handler;
    file.setFileName("ole.bull");
    QCOMPARE(file.size(), qint64(124));

    // A new, identical handler should take preference over the last one.
    MyHandler2 handler2;
    file.setFileName("ole.bull");
    QCOMPARE(file.size(), qint64(125));
#endif
}

#ifdef QT_BUILD_INTERNAL
class MyRecursiveHandler : public QAbstractFileEngineHandler
{
    Q_DISABLE_COPY_MOVE(MyRecursiveHandler)
public:
    MyRecursiveHandler() = default;

    std::unique_ptr<QAbstractFileEngine> create(const QString &fileName) const override
    {
        if (fileName.startsWith(":!")) {
            QDir dir;

#ifndef BUILTIN_TESTDATA
            const QString realFile = QFINDTESTDATA(fileName.mid(2));
#else
            const QString realFile = m_dataDir->filePath(fileName.mid(2));
#endif
            if (dir.exists(realFile))
                return std::make_unique<QFSFileEngine>(realFile);
        }
        return nullptr;
    }

#ifdef BUILTIN_TESTDATA
    QSharedPointer<QTemporaryDir> m_dataDir;
#endif
};
#endif

#ifdef QT_BUILD_INTERNAL
void tst_QFile::useQFileInAFileHandler()
{
    // This test should not dead-lock
    MyRecursiveHandler handler;
#ifdef BUILTIN_TESTDATA
    handler.m_dataDir = m_dataDir;
#endif
    QFile file(":!tst_qfile.cpp");
    QVERIFY(file.exists());
}
#endif

void tst_QFile::getCharFF()
{
    QFile file("file.txt");
    QVERIFY(file.open(QFile::ReadWrite));
    file.write("\xff\xff\xff");
    file.flush();
    file.seek(0);

    char c;
    QVERIFY(file.getChar(&c));
    QVERIFY(file.getChar(&c));
    QVERIFY(file.getChar(&c));
}

void tst_QFile::remove_and_exists()
{
    QFile::remove("tull_i_grunn.txt");
    QFile f("tull_i_grunn.txt");

    QVERIFY(!f.exists());

    bool opened = f.open(QIODevice::WriteOnly);
    QVERIFY(opened);

    f.write("testing that remove/exists work...");
    f.close();

    QVERIFY(f.exists());

    f.remove();
    QVERIFY(!f.exists());
}

void tst_QFile::removeOpenFile()
{
    {
        // remove an opened, write-only file
        QFile::remove("remove_unclosed.txt");
        QFile f("remove_unclosed.txt");

        QVERIFY(!f.exists());
        bool opened = f.open(QIODevice::WriteOnly);
        QVERIFY(opened);
        f.write("testing that remove closes the file first...");

        bool removed = f.remove(); // remove should both close and remove the file
        QVERIFY(removed);
        QVERIFY(!f.isOpen());
        QVERIFY(!f.exists());
        QCOMPARE(f.error(), QFile::NoError);
    }

    {
        // remove an opened, read-only file
        QFile::remove("remove_unclosed.txt");

        // first, write a file that we can remove
        {
            QFile f("remove_unclosed.txt");
            QVERIFY(!f.exists());
            bool opened = f.open(QIODevice::WriteOnly);
            QVERIFY(opened);
            f.write("testing that remove closes the file first...");
            f.close();
        }

        QFile f("remove_unclosed.txt");
        bool opened = f.open(QIODevice::ReadOnly);
        QVERIFY(opened);
        f.readAll();
        // this used to only fail on FreeBSD (and OS X)
        QVERIFY(f.flush());
        bool removed = f.remove(); // remove should both close and remove the file
        QVERIFY(removed);
        QVERIFY(!f.isOpen());
        QVERIFY(!f.exists());
        QCOMPARE(f.error(), QFile::NoError);
    }
}

void tst_QFile::fullDisk()
{
    QFile file("/dev/full");
    if (!file.exists())
        QSKIP("/dev/full doesn't exist on this system");

    QVERIFY2(file.open(QIODevice::WriteOnly), msgOpenFailed(file).constData());
    file.write("foobar", 6);

    QVERIFY(!file.flush());
    QCOMPARE(file.error(), QFile::ResourceError);
    QVERIFY(!file.flush());
    QCOMPARE(file.error(), QFile::ResourceError);

    char c = 0;
    file.write(&c, 0);
    QVERIFY(!file.flush());
    QCOMPARE(file.error(), QFile::ResourceError);
    QCOMPARE(file.write(&c, 1), qint64(1));
    QVERIFY(!file.flush());
    QCOMPARE(file.error(), QFile::ResourceError);

    file.close();
    QVERIFY(!file.isOpen());
    QCOMPARE(file.error(), QFile::ResourceError);

    QVERIFY2(file.open(QIODevice::WriteOnly), msgOpenFailed(file).constData());
    QCOMPARE(file.error(), QFile::NoError);
    QVERIFY(file.flush()); // Shouldn't inherit write buffer
    file.close();
    QCOMPARE(file.error(), QFile::NoError);

    // try again without flush:
    QVERIFY2(file.open(QIODevice::WriteOnly), msgOpenFailed(file).constData());
    file.write("foobar", 6);
    file.close();
    QVERIFY(file.error() != QFile::NoError);
}

void tst_QFile::writeLargeDataBlock_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("type");

    QTest::newRow("localfile-QFile")  << "./largeblockfile.txt" << (int)OpenQFile;
    QTest::newRow("localfile-Fd")     << "./largeblockfile.txt" << (int)OpenFd;
    QTest::newRow("localfile-Stream") << "./largeblockfile.txt" << (int)OpenStream;

#if defined(Q_OS_WIN) && !defined(QT_NO_NETWORK)
    // Some semi-randomness to avoid collisions.
    QTest::newRow("unc file")
        << QString("//" + QTest::uncServerName() + "/TESTSHAREWRITABLE/largefile-%1-%2.txt")
        .arg(QHostInfo::localHostName())
        .arg(QTime::currentTime().msec()) << (int)OpenQFile;
#endif
}

static QByteArray getLargeDataBlock()
{
    static QByteArray array;

    if (array.isNull())
    {
#if defined(Q_OS_VXWORKS)
        int resizeSize = 1024 * 1024; // VxWorks does not have much space
#else
        int resizeSize = 64 * 1024 * 1024;
#endif
        array.resize(resizeSize);
        for (int i = 0; i < array.size(); ++i)
            array[i] = uchar(i);
    }

    return array;
}

void tst_QFile::writeLargeDataBlock()
{
    QFETCH(QString, fileName);
    QFETCH( int, type );

    QByteArray const originalData = getLargeDataBlock();

    {
        QFile file(fileName);

        QVERIFY2(openFile(file, QIODevice::WriteOnly, (FileType)type), msgOpenFailed(file));
        qint64 fileWriteOriginalData = file.write(originalData);
        qint64 originalDataSize      = (qint64)originalData.size();
#if defined(Q_OS_WIN)
        if (fileWriteOriginalData != originalDataSize) {
            qWarning() << qPrintable(QString("Error writing a large data block to [%1]: %2")
                .arg(fileName)
                .arg(file.errorString()));
            QEXPECT_FAIL("unc file", "QTBUG-26906 writing", Abort);
        }
#endif
        QCOMPARE( fileWriteOriginalData, originalDataSize );
        QVERIFY( file.flush() );

        closeFile(file);
    }

    QByteArray readData;

    {
        QFile file(fileName);

        QVERIFY2( openFile(file, QIODevice::ReadOnly, (FileType)type),
            qPrintable(QString("Couldn't open file for reading: [%1]").arg(fileName)) );
        readData = file.readAll();

#if defined(Q_OS_WIN)
        if (readData != originalData) {
            qWarning() << qPrintable(QString("Error reading a large data block from [%1]: %2")
                .arg(fileName)
                .arg(file.errorString()));
            QEXPECT_FAIL("unc file", "QTBUG-26906 reading", Abort);
        }
#endif
        closeFile(file);
    }
    QCOMPARE( readData, originalData );
    QVERIFY( QFile::remove(fileName) );
}

void tst_QFile::readFromWriteOnlyFile()
{
    QFile file("writeonlyfile");
    QVERIFY2(file.open(QFile::WriteOnly), msgOpenFailed(file).constData());
    char c;
    QTest::ignoreMessage(QtWarningMsg, "QIODevice::read (QFile, \"writeonlyfile\"): WriteOnly device");
    QCOMPARE(file.read(&c, 1), qint64(-1));
}

void tst_QFile::writeToReadOnlyFile()
{
    QFile file("readonlyfile");
    QVERIFY2(file.open(QFile::ReadOnly), msgOpenFailed(file).constData());
    char c = 0;
    QTest::ignoreMessage(QtWarningMsg, "QIODevice::write (QFile, \"readonlyfile\"): ReadOnly device");
    QCOMPARE(file.write(&c, 1), qint64(-1));
}

#if defined(Q_OS_LINUX)
// This platform have 0-sized virtual files
void tst_QFile::virtualFile_data()
{
    QTest::addColumn<QIODevice::OpenMode>("mode");
    QTest::newRow("buffered") << QIODevice::OpenMode();
    QTest::newRow("unbuffered") << QIODevice::OpenMode(QIODevice::Unbuffered);
}

void tst_QFile::virtualFile()
{
    QFETCH(QIODevice::OpenMode, mode);

    // We need to test a large-ish /proc file on Linux, one that is usually
    // over 4 kB (because the kernel writes in chunks of that), has a
    // cross-platform file format, and is definitely readable. The best
    // candidate and the one we can verify anything in is /proc/<PID>/maps.
    // However, our act of reading may change the map because we allocate
    // memory, so we fork() here so we have a frozen snapshot of the file.

    int efd = eventfd(0, EFD_CLOEXEC);
    pid_t pid = fork();
    if (pid == 0) {
        // child
        uint64_t val;
        eventfd_read(efd, &val);
        _exit(0);
    }
    QVERIFY2(pid > 0, "fork failed: " + qt_error_string().toLocal8Bit());
    auto waitForChild = qScopeGuard([=] {
        eventfd_write(efd, 1);
        close(efd);
        waitpid(pid, nullptr, 0);
    });

    QString fname = u"/proc/%1/maps"_s.arg(pid);

    // consistency check
    QFileInfo fi(fname);
    QVERIFY2(fi.exists(), msgFileDoesNotExist(fname).constData());
    QVERIFY(fi.isFile());
    QCOMPARE(fi.size(), Q_INT64_C(0));

    // open the file
    QFile f(fname);
    QVERIFY2(f.open(QIODevice::ReadOnly | mode), msgOpenFailed(f).constData());
    QCOMPARE(f.size(), Q_INT64_C(0));
    QVERIFY(f.atEnd());

    // read data
    QByteArray data = f.read(16);
    QCOMPARE(data.size(), 16);
    QCOMPARE(f.pos(), Q_INT64_C(16));

    // seeking
    QVERIFY(f.seek(1));
    QCOMPARE(f.pos(), Q_INT64_C(1));
    QVERIFY(f.seek(0));
    QCOMPARE(f.pos(), Q_INT64_C(0));

    // line-reading
    QList<QByteArray> lines;
    for (data = f.readLine(); !data.isEmpty(); data = f.readLine()) {
        // chop the newline -- not using .trimmed() so cut exactly one byte
        data.chop(1);
        lines += std::move(data);
    }

    if (!QT_CONFIG(static) && !QTestPrivate::isRunningArmOnX86()) {
        // we must be able to find QtCore and QtTest somewhere
        static const char corelib[] = "libQt" QT_STRINGIFY(QT_VERSION_MAJOR) "Core";
        static const char testlib[] = "libQt" QT_STRINGIFY(QT_VERSION_MAJOR) "Test";
        auto contains = [&](QByteArrayView text, quintptr ptr = 0) {
            // this is not the same a QList::contains()
            return std::any_of(lines.constBegin(), lines.constEnd(), [=](QByteArrayView entry) {
                if (!entry.contains(text))
                    return false;
                if (!ptr)
                    return true;
                qsizetype dash = entry.indexOf('-');
                qsizetype space = entry.indexOf(' ', dash);
                quintptr start = entry.left(dash).toULong(nullptr, 16);
                quintptr end = entry.left(space).mid(dash + 1).toULong(nullptr, 16);
                return start <= ptr && ptr <= end;
            });
        };
        QVERIFY(contains(corelib, quintptr(f.metaObject())));
        QVERIFY(contains(testlib));
    }

    // read all:
    QVERIFY(f.seek(0));
    data = f.readAll();
    QVERIFY(f.pos() != 0);
    QVERIFY(!data.isEmpty());

    QCOMPARE(data, lines.join('\n') + '\n');
}
#endif // defined(Q_OS_LINUX) || defined(Q_OS_AIX) || defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD)

#if defined (Q_OS_UNIX) && !defined(Q_OS_WASM)
// wasm does not have working fifo
//    https://github.com/nodejs/node/issues/38344
// wasm does not have blocking pipe I/O
//    https://github.com/emscripten-core/emscripten/issues/13214
// wasm does not, by default, have socketpair
//    https://emscripten.org/docs/porting/networking.html
static void unixPipe_helper(int pipes[2])
{
    // start a thread and wait for it to write a first byte
    static constexpr int Timeout = 1000;
    QScopedPointer<QThread> thr(QThread::create([fd = pipes[1]]() {
        char c = 1;
        qt_safe_write(fd, &c, 1);
        QTest::qSleep(Timeout);
        c = 2;
        qt_safe_write(fd, &c, 1);
    }));

    thr->start();

    // synchronize with the thread having started
    char c = 0;
    QVERIFY2(qt_safe_read(pipes[0], &c, 1) == 1, qPrintable(qt_error_string()));
    QCOMPARE(c, '\1');

    QFETCH(bool, useStdio);
    QFile f;
    if (useStdio) {
        FILE *fh = fdopen(pipes[0], "rb");
        QVERIFY(f.open(fh, QIODevice::ReadOnly | QIODevice::Unbuffered, QFileDevice::AutoCloseHandle));
        pipes[0] = -1;      // QFile fclose()s the FILE* and that close()s the fd
    } else {
        QVERIFY(f.open(pipes[0], QIODevice::ReadOnly | QIODevice::Unbuffered));
    }

    // this ought to block
    c = 0;
    QCOMPARE(f.read(&c, 1), 1);
    QCOMPARE(c, '\2');

    thr->wait();
}

void tst_QFile::unixPipe_data()
{
    QTest::addColumn<bool>("useStdio");
    QTest::newRow("no-stdio") << false;
    QTest::newRow("with-stdio") << true;
}

void tst_QFile::unixPipe()
{
    int pipes[2] = { -1, -1 };
    QVERIFY2(pipe(pipes) == 0, qPrintable(qt_error_string()));
    unixPipe_helper(pipes);
    if (pipes[0] != -1)
        qt_safe_close(pipes[0]);
    qt_safe_close(pipes[1]);
}

void tst_QFile::unixFifo()
{
    QByteArray fifopath = []() -> QByteArray {
        QByteArray dir = qgetenv("XDG_RUNTIME_DIR");
        if (dir.isEmpty())
            dir = QFile::encodeName(QDir::tempPath());

        // try to create a FIFO
        for (int attempts = 10; attempts; --attempts) {
            QByteArray fifopath = dir + "/tst_qfile_fifo." +
                    QByteArray::number(QRandomGenerator::global()->generate());
            int ret = mkfifo(fifopath, 0600);
            if (ret == 0)
                return fifopath;
        }

        qWarning("Failed to create a FIFO at %s; last error was %s",
                 dir.constData(), strerror(errno));
        return {};
    }();
    if (fifopath.isEmpty())
        return;

    auto removeFifo = qScopeGuard([&fifopath] { unlink(fifopath); });

    // with a FIFO, the two open() system calls synchronize
    QScopedPointer<QThread> thr(QThread::create([&fifopath]() {
        int fd = qt_safe_open(fifopath, O_WRONLY);
        QTest::qSleep(500);
        char c = 2;
        qt_safe_write(fd, &c, 1);
        qt_safe_close(fd);
    }));
    thr->start();

    QFETCH(bool, useStdio);
    QFile f;
    if (useStdio) {
        FILE *fh = fopen(fifopath, "rb");
        QVERIFY(f.open(fh, QIODevice::ReadOnly | QIODevice::Unbuffered, QFileDevice::AutoCloseHandle));
    } else {
        f.setFileName(QFile::decodeName(fifopath));
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Unbuffered));
    }

    char c = 0;
    QCOMPARE(f.read(&c, 1), 1);         // this ought to block
    QCOMPARE(c, '\2');
    thr->wait();
}

void tst_QFile::socketPair()
{
#if defined(Q_OS_VXWORKS)
    QSKIP("socketpair is not available on Vxworks");
#else
    int pipes[2] = { -1, -1 };
    QVERIFY2(socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) == 0, qPrintable(qt_error_string()));
    unixPipe_helper(pipes);
    if (pipes[0] != -1)
        qt_safe_close(pipes[0]);
    qt_safe_close(pipes[1]);
#endif
}
#endif /* UNIX && !WASM; */

void tst_QFile::textFile()
{
    // The "t" is ignored everywhere except on Windows
    StdioFileGuard fs(fopen("writeabletextfile", "wt"));
    QVERIFY(fs);
    QFile f;
    QByteArray part1("This\nis\na\nfile\nwith\nnewlines\n");
    QByteArray part2("Add\nsome\nmore\nnewlines\n");

    QVERIFY(f.open(fs, QIODevice::WriteOnly));
    f.write(part1);
    f.write(part2);
    f.close();
    fs.close();

    QFile file("writeabletextfile");
    QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());

    QByteArray data = file.readAll();

    QByteArray expected = part1 + part2;
#ifdef Q_OS_WIN
    expected.replace("\n", "\015\012");
#endif
    QCOMPARE(data, expected);
    file.close();
}

static const char renameSourceFile[] = "renamefile";

void tst_QFile::rename_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("destination");
    QTest::addColumn<bool>("result");

    QTest::newRow("a -> b") << QString("a") << QString("b") << false;
    QTest::newRow("a -> .") << QString("a") << QString(".") << false;
    QTest::newRow("renamefile -> renamefile") << QString::fromLatin1(renameSourceFile) << QString::fromLatin1(renameSourceFile) << false;
    QTest::newRow("renamefile -> noreadfile") << QString::fromLatin1(renameSourceFile) << QString::fromLatin1(noReadFile) << false;
#if defined(Q_OS_UNIX)
    QTest::newRow("renamefile -> /etc/renamefile") << QString::fromLatin1(renameSourceFile) << QString("/etc/renamefile") << false;
#endif
    QTest::newRow("renamefile -> renamedfile") << QString::fromLatin1(renameSourceFile) << QString("renamedfile") << true;
    QTest::newRow("renamefile -> ..") << QString::fromLatin1(renameSourceFile) << QString("..") << false;
    QTest::newRow("renamefile -> rEnAmEfIlE") << QString::fromLatin1(renameSourceFile) << QStringLiteral("rEnAmEfIlE") << true;
}

void tst_QFile::rename()
{
    QFETCH(QString, source);
    QFETCH(QString, destination);
    QFETCH(bool, result);

    const QByteArray content = QByteArrayLiteral("testdatacontent") + QTime::currentTime().toString().toLatin1();

#if defined(Q_OS_UNIX)
    if (strcmp(QTest::currentDataTag(), "renamefile -> /etc/renamefile") == 0) {
#if !defined(Q_OS_VXWORKS)
        if (::getuid() == 0)
#endif
            QSKIP("Running this test as root doesn't make sense");
    }
#endif

    const QString sourceFileName = QString::fromLatin1(renameSourceFile);
    QFile sourceFile(sourceFileName);
    QVERIFY2(sourceFile.open(QFile::WriteOnly | QFile::Text), qPrintable(sourceFile.errorString()));
    QVERIFY2(sourceFile.write(content), qPrintable(sourceFile.errorString()));
    sourceFile.close();

    QFile file(source);
    const bool success = file.rename(destination);
    if (result) {
        QVERIFY2(success, qPrintable(file.errorString()));
        QCOMPARE(file.error(), QFile::NoError);
        // This will report the source file still existing for a rename changing the case
        // on Windows, Mac.
        if (sourceFileName.compare(destination, Qt::CaseInsensitive))
            QVERIFY(!sourceFile.exists());
        QFile destinationFile(destination);
        QVERIFY2(destinationFile.open(QFile::ReadOnly | QFile::Text), qPrintable(destinationFile.errorString()));
        QCOMPARE(destinationFile.readAll(), content);
        destinationFile.close();
    } else {
        QVERIFY(!success);
        QCOMPARE(file.error(), QFile::RenameError);
    }
}

/*!
 \since 4.5

 Some special files have QFile::atEnd() returning true, even though there is
 more data available. True for corner cases, as well as some mounts on \macos.

 Here, we reproduce that condition by having a QFile sub-class with this
 peculiar atEnd() behavior.
*/
void tst_QFile::renameWithAtEndSpecialFile() const
{
    class PeculiarAtEnd : public QFile
    {
    public:
        virtual bool atEnd() const override
        {
            return true;
        }
    };

    const QString newName(QLatin1String("newName.txt"));
    /* Cleanup, so we're a bit more robust. */
    QFile::remove(newName);

    const QString originalName = QStringLiteral("forRenaming.txt");
    // Copy from source tree
    if (!QFile::exists(originalName))
        QVERIFY(QFile::copy(m_forRenamingFile, originalName));

    PeculiarAtEnd file;
    file.setFileName(originalName);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

    QVERIFY(file.rename(newName));

    file.close();
}

void tst_QFile::renameFallback()
{
    // Using a resource file both to trigger QFile::rename's fallback handling
    // and as a *read-only* source whose move should fail.
    QFile file(":/rename-fallback.qrc");
    QVERIFY(file.exists() && "(test-precondition)");
    QFile::remove("file-rename-destination.txt");

    QVERIFY(!file.rename("file-rename-destination.txt"));
    QVERIFY(!QFile::exists("file-rename-destination.txt"));
    QVERIFY(!file.isOpen());
}

void tst_QFile::renameMultiple()
{
    // create the file if it doesn't exist
    QFile file("file-to-be-renamed.txt");
    QFile file2("existing-file.txt");
    QVERIFY2(file.open(QIODevice::ReadWrite), msgOpenFailed(file).constData());
    QVERIFY2(file2.open(QIODevice::ReadWrite), msgOpenFailed(file2).constData());

    // any stale files from previous test failures?
    QFile::remove("file-renamed-once.txt");
    QFile::remove("file-renamed-twice.txt");

    // begin testing
    QVERIFY(QFile::exists("existing-file.txt"));
    QVERIFY(!file.rename("existing-file.txt"));
    QCOMPARE(file.error(), QFile::RenameError);
    QCOMPARE(file.fileName(), QString("file-to-be-renamed.txt"));

    QVERIFY(file.rename("file-renamed-once.txt"));
    QVERIFY(!file.isOpen());
    QCOMPARE(file.fileName(), QString("file-renamed-once.txt"));

    QVERIFY(QFile::exists("existing-file.txt"));
    QVERIFY(!file.rename("existing-file.txt"));
    QCOMPARE(file.error(), QFile::RenameError);
    QCOMPARE(file.fileName(), QString("file-renamed-once.txt"));

    QVERIFY(file.rename("file-renamed-twice.txt"));
    QVERIFY(!file.isOpen());
    QCOMPARE(file.fileName(), QString("file-renamed-twice.txt"));

    QVERIFY(QFile::exists("existing-file.txt"));
    QVERIFY(!QFile::exists("file-to-be-renamed.txt"));
    QVERIFY(!QFile::exists("file-renamed-once.txt"));
    QVERIFY(QFile::exists("file-renamed-twice.txt"));

    file.remove();
    file2.remove();
    QVERIFY(!QFile::exists("file-renamed-twice.txt"));
    QVERIFY(!QFile::exists("existing-file.txt"));
}

void tst_QFile::appendAndRead()
{
    const QString fileName(QStringLiteral("appendfile.txt"));
    QFile writeFile(fileName);
    QVERIFY2(writeFile.open(QIODevice::Append | QIODevice::Truncate), msgOpenFailed(writeFile).constData());

    QFile readFile(fileName);
    QVERIFY2(readFile.open(QIODevice::ReadOnly), msgOpenFailed(readFile).constData());

    // Write to the end of the file, then read that character back, and so on.
    for (int i = 0; i < 100; ++i) {
        char c = '\0';
        writeFile.putChar(char(i));
        writeFile.flush();
        QVERIFY(readFile.getChar(&c));
        QCOMPARE(c, char(i));
        QCOMPARE(readFile.pos(), writeFile.pos());
    }

    // Write blocks and read them back
    for (int j = 0; j < 18; ++j) {
        const int size = 1 << j;
        writeFile.write(QByteArray(size, '@'));
        writeFile.flush();
        QCOMPARE(readFile.read(size).size(), size);
    }
}

void tst_QFile::miscWithUncPathAsCurrentDir()
{
#if defined(Q_OS_WIN)
    QString current = QDir::currentPath();
    const QString path = QLatin1String("//") + QTest::uncServerName()
        + QLatin1String("/testshare");
    QVERIFY2(QDir::setCurrent(path), qPrintable(QDir::toNativeSeparators(path)));
    QFile file("test.pri");
    QVERIFY2(file.exists(), msgFileDoesNotExist(file.fileName()).constData());
    QCOMPARE(int(file.size()), 34);
    QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());
    QVERIFY(QDir::setCurrent(current));
#endif
}

void tst_QFile::standarderror()
{
    QFile f;
    bool ok = f.open(stderr, QFile::WriteOnly);
    QVERIFY(ok);
    f.close();
}

void tst_QFile::handle()
{
    int fd;
    QFile file(m_testSourceFile);
    QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());
    fd = int(file.handle());
    QVERIFY(fd > 2);
    QCOMPARE(int(file.handle()), fd);
    char c = '\0';
    {
        const auto readResult = QT_READ(int(file.handle()), &c, 1);
        decltype(readResult) expected = 1;
        QCOMPARE(readResult, expected);
    }
    QCOMPARE(c, '/');

    // test if the QFile and the handle remain in sync
    QVERIFY(file.getChar(&c));
    QCOMPARE(c, '/');

    // same, but read from QFile first now
    file.close();
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Unbuffered), msgOpenFailed(file).constData());
    fd = int(file.handle());
    QVERIFY(fd > 2);
    QVERIFY(file.getChar(&c));
    QCOMPARE(c, '/');
#ifdef Q_OS_UNIX
    QCOMPARE(QT_READ(fd, &c, 1), ssize_t(1));
#else
    QCOMPARE(QT_READ(fd, &c, 1), 1);
#endif

    QCOMPARE(c, '/');

    //test round trip of adopted stdio file handle
    QFile file2;
    StdioFileGuard fp(fopen(qPrintable(m_testSourceFile), "r"));
    QVERIFY(fp);
    QVERIFY(file2.open(fp, QIODevice::ReadOnly));
    QCOMPARE(int(file2.handle()), int(QT_FILENO(fp)));
    QCOMPARE(int(file2.handle()), int(QT_FILENO(fp)));
    fp.close();

    //test round trip of adopted posix file handle
#ifdef Q_OS_UNIX
    QFile file3;
    fd = QT_OPEN(qPrintable(m_testSourceFile), QT_OPEN_RDONLY);
    QVERIFY(file3.open(fd, QIODevice::ReadOnly));
    QCOMPARE(int(file3.handle()), fd);
    QT_CLOSE(fd);
#endif
}

void tst_QFile::nativeHandleLeaks()
{
    int fd1, fd2;

#ifdef Q_OS_WIN
    HANDLE handle1, handle2;
#endif

    {
        QFile file("qt_file.tmp");
        QVERIFY2(file.open(QIODevice::ReadWrite), msgOpenFailed(file).constData());

        fd1 = file.handle();
        QVERIFY( -1 != fd1 );
    }

#ifdef Q_OS_WIN
    handle1 = ::CreateFileA("qt_file.tmp", GENERIC_READ, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    QVERIFY( INVALID_HANDLE_VALUE != handle1 );
    QVERIFY( ::CloseHandle(handle1) );
#endif

    {
        QFile file("qt_file.tmp");
        QVERIFY2(file.open(QIODevice::ReadOnly), msgOpenFailed(file).constData());

        fd2 = file.handle();
        QVERIFY( -1 != fd2 );
    }

#ifdef Q_OS_WIN
    handle2 = ::CreateFileA("qt_file.tmp", GENERIC_READ, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    QVERIFY( INVALID_HANDLE_VALUE != handle2 );
    QVERIFY( ::CloseHandle(handle2) );
#endif

    QCOMPARE( fd2, fd1 );
}

void tst_QFile::readEof_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("imode");

    QTest::newRow("buffered") << m_testFile << 0;
    QTest::newRow("unbuffered") << m_testFile << int(QIODevice::Unbuffered);

#if defined(Q_OS_UNIX)
    QTest::newRow("sequential,buffered") << "/dev/null" << 0;
    QTest::newRow("sequential,unbuffered") << "/dev/null" << int(QIODevice::Unbuffered);
#endif
}

void tst_QFile::readEof()
{
    QFETCH(QString, filename);
    QFETCH(int, imode);
    QIODevice::OpenMode mode = QIODevice::OpenMode(imode);

    {
        QFile file(filename);
        QVERIFY2(file.open(QIODevice::ReadOnly | mode), msgOpenFailed(file).constData());
        bool isSequential = file.isSequential();
        if (!isSequential) {
            QVERIFY(file.seek(245));
            QVERIFY(file.atEnd());
        }

        char buf[10];
        int ret = file.read(buf, sizeof buf);
        QCOMPARE(ret, 0);
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());

        // Do it again to ensure that we get the same result
        ret = file.read(buf, sizeof buf);
        QCOMPARE(ret, 0);
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());
    }

    {
        QFile file(filename);
        QVERIFY2(file.open(QIODevice::ReadOnly | mode), msgOpenFailed(file).constData());
        bool isSequential = file.isSequential();
        if (!isSequential) {
            QVERIFY(file.seek(245));
            QVERIFY(file.atEnd());
        }

        QByteArray ret = file.read(10);
        QVERIFY(ret.isEmpty());
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());

        // Do it again to ensure that we get the same result
        ret = file.read(10);
        QVERIFY(ret.isEmpty());
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());
    }

    {
        QFile file(filename);
        QVERIFY2(file.open(QIODevice::ReadOnly | mode), msgOpenFailed(file).constData());
        bool isSequential = file.isSequential();
        if (!isSequential) {
            QVERIFY(file.seek(245));
            QVERIFY(file.atEnd());
        }

        char buf[10];
        int ret = file.readLine(buf, sizeof buf);
        QCOMPARE(ret, -1);
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());

        // Do it again to ensure that we get the same result
        ret = file.readLine(buf, sizeof buf);
        QCOMPARE(ret, -1);
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());
    }

    {
        QFile file(filename);
        QVERIFY2(file.open(QIODevice::ReadOnly | mode), msgOpenFailed(file).constData());
        bool isSequential = file.isSequential();
        if (!isSequential) {
            QVERIFY(file.seek(245));
            QVERIFY(file.atEnd());
        }

        QByteArray ret = file.readLine();
        QVERIFY(ret.isNull());
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());

        // Do it again to ensure that we get the same result
        ret = file.readLine();
        QVERIFY(ret.isNull());
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());
    }

    {
        QFile file(filename);
        QVERIFY2(file.open(QIODevice::ReadOnly | mode), msgOpenFailed(file).constData());
        bool isSequential = file.isSequential();
        if (!isSequential) {
            QVERIFY(file.seek(245));
            QVERIFY(file.atEnd());
        }

        char c;
        QVERIFY(!file.getChar(&c));
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());

        // Do it again to ensure that we get the same result
        QVERIFY(!file.getChar(&c));
        QCOMPARE(file.error(), QFile::NoError);
        QVERIFY(file.atEnd());
    }
}

void tst_QFile::posAfterFailedStat()
{
    // Regression test for a bug introduced in 4.3.0; after a failed stat,
    // pos() could no longer be calculated correctly.
    QFile::remove("tmp.txt");
    QFile file("tmp.txt");
    QVERIFY(!file.exists());
    QVERIFY2(file.open(QIODevice::Append), msgOpenFailed(file).constData());
    QVERIFY(file.exists());
    file.write("qt430", 5);
    QVERIFY(!file.isSequential());
    QCOMPARE(file.pos(), qint64(5));
    file.remove();
}

#define FILESIZE 65536 * 3

void tst_QFile::map_data()
{
    QTest::addColumn<int>("fileSize");
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("size");
    QTest::addColumn<QFile::FileError>("error");

    QTest::newRow("zero")         << FILESIZE << 0     << FILESIZE         << QFile::NoError;
    QTest::newRow("small, but 0") << FILESIZE << 30    << FILESIZE - 30    << QFile::NoError;
    QTest::newRow("a page")       << FILESIZE << 4096  << FILESIZE - 4096  << QFile::NoError;
    QTest::newRow("+page")        << FILESIZE << 5000  << FILESIZE - 5000  << QFile::NoError;
    QTest::newRow("++page")       << FILESIZE << 65576 << FILESIZE - 65576 << QFile::NoError;
    QTest::newRow("bad size")     << FILESIZE << 0     << -1               << QFile::ResourceError;
    QTest::newRow("bad offset")   << FILESIZE << -1    << 1                << QFile::UnspecifiedError;
    QTest::newRow("zerozero")     << FILESIZE << 0     << 0                << QFile::UnspecifiedError;
}

void tst_QFile::map()
{
    QFETCH(int, fileSize);
    QFETCH(int, offset);
    QFETCH(int, size);
    QFETCH(QFile::FileError, error);

    QString fileName = QDir::currentPath() + '/' + "qfile_map_testfile";

    if (QFile::exists(fileName)) {
        QVERIFY(QFile::setPermissions(fileName,
            QFile::WriteOwner | QFile::ReadOwner | QFile::WriteUser | QFile::ReadUser));
        QFile::remove(fileName);
    }
    QFile file(fileName);

    // invalid, not open
    uchar *memory = file.map(0, size);
    QVERIFY(!memory);
    QCOMPARE(file.error(), QFile::PermissionsError);
    QVERIFY(!file.unmap(memory));
    QCOMPARE(file.error(), QFile::PermissionsError);

    // make a file
    QVERIFY2(file.open(QFile::ReadWrite), msgOpenFailed(file).constData());
    QVERIFY(file.resize(fileSize));
    QVERIFY(file.flush());
    file.close();
    QVERIFY2(file.open(QFile::ReadWrite), msgOpenFailed(file).constData());
    memory = file.map(offset, size);
    if (error != QFile::NoError) {
        QVERIFY(file.error() != QFile::NoError);
        return;
    }

    QCOMPARE(file.error(), error);
    QVERIFY(memory);
    memory[0] = 'Q';
    QVERIFY(file.unmap(memory));
    QCOMPARE(file.error(), QFile::NoError);

    // Verify changes were saved
    memory = file.map(offset, size);
    QCOMPARE(file.error(), QFile::NoError);
    QVERIFY(memory);
    QCOMPARE(memory[0], uchar('Q'));
    QVERIFY(file.unmap(memory));
    QCOMPARE(file.error(), QFile::NoError);

    // hpux won't let you map multiple times.
#if !defined(Q_OS_HPUX) && !defined(Q_USE_DEPRECATED_MAP_API)
    // exotic test to make sure that multiple maps work

    // note: windows ce does not reference count mutliple maps
    // it's essentially just the same reference but it
    // cause a resource lock on the file which prevents it
    // from being removed    uchar *memory1 = file.map(0, file.size());
    uchar *memory1 = file.map(0, file.size());
    QCOMPARE(file.error(), QFile::NoError);
    uchar *memory2 = file.map(0, file.size());
    QCOMPARE(file.error(), QFile::NoError);
    QVERIFY(memory1);
    QVERIFY(memory2);
    QVERIFY(file.unmap(memory1));
    QCOMPARE(file.error(), QFile::NoError);
    QVERIFY(file.unmap(memory2));
    QCOMPARE(file.error(), QFile::NoError);
    memory1 = file.map(0, file.size());
    QCOMPARE(file.error(), QFile::NoError);
    QVERIFY(memory1);
    QVERIFY(file.unmap(memory1));
    QCOMPARE(file.error(), QFile::NoError);
#endif

    file.close();

#if !defined(Q_OS_VXWORKS)
#if defined(Q_OS_UNIX)
    if (::getuid() != 0)
        // root always has permissions
#endif
    {
        // Change permissions on a file, just to confirm it would fail
        QFile::Permissions originalPermissions = file.permissions();
        QVERIFY(file.setPermissions(QFile::ReadOther));
        QVERIFY(!file.open(QFile::ReadWrite));
        memory = file.map(offset, size);
        QCOMPARE(file.error(), QFile::PermissionsError);
        QVERIFY(!memory);
        QVERIFY(file.setPermissions(originalPermissions));
    }
#endif
    QVERIFY(file.remove());
}

void tst_QFile::mapResource_data()
{
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("size");
    QTest::addColumn<QFile::FileError>("error");
    QTest::addColumn<QString>("fileName");

    QString validFile = ":/tst_qfile/resources/file1.ext1";
    QString invalidFile = ":/tst_qfile/resources/filefoo.ext1";
    const char modes[] = "invalid";

    for (int i = 0; i < 2; ++i) {
        QString file = (i == 0) ? validFile : invalidFile;
        const char *mode = i == 0 ? modes + 2 : modes;
        QTest::addRow("0, 0 (%s)", mode) << 0 << 0 << QFile::UnspecifiedError << file;
        QTest::addRow("0, BIG (%s)", mode) << 0 << 4096 << QFile::UnspecifiedError << file;
        QTest::addRow("-1, 0 (%s)", mode) << -1 << 0 << QFile::UnspecifiedError << file;
        QTest::addRow("0, -1 (%s)", mode) << 0 << -1 << QFile::UnspecifiedError << file;
    }

    QTest::newRow("0, 1") << 0 << 1 << QFile::NoError << validFile;
}

void tst_QFile::mapResource()
{
    QFETCH(QString, fileName);
    QFETCH(int, offset);
    QFETCH(int, size);
    QFETCH(QFile::FileError, error);

    QFile file(fileName);
    uchar *memory = file.map(offset, size);
    QCOMPARE(file.error(), error);
    QVERIFY((error == QFile::NoError) ? (memory != 0) : (memory == 0));
    if (error == QFile::NoError)
        QCOMPARE(QString(QChar(memory[0])), QString::number(offset + 1));
    QVERIFY(file.unmap(memory));
}

void tst_QFile::mapOpenMode_data()
{
    QTest::addColumn<int>("openMode");
    QTest::addColumn<int>("flags");

    QTest::newRow("ReadOnly") << int(QIODevice::ReadOnly) << int(QFileDevice::NoOptions);
    //QTest::newRow("WriteOnly") << int(QIODevice::WriteOnly); // this doesn't make sense
    QTest::newRow("ReadWrite") << int(QIODevice::ReadWrite) << int(QFileDevice::NoOptions);
    QTest::newRow("ReadOnly,Unbuffered") << int(QIODevice::ReadOnly | QIODevice::Unbuffered) << int(QFileDevice::NoOptions);
    QTest::newRow("ReadWrite,Unbuffered") << int(QIODevice::ReadWrite | QIODevice::Unbuffered) << int(QFileDevice::NoOptions);
    QTest::newRow("ReadOnly + MapPrivate") << int(QIODevice::ReadOnly) << int(QFileDevice::MapPrivateOption);
    QTest::newRow("ReadWrite + MapPrivate") << int(QIODevice::ReadWrite) << int(QFileDevice::MapPrivateOption);
}

void tst_QFile::mapOpenMode()
{
    QFETCH(int, openMode);
    QFETCH(int, flags);
    static const qint64 fileSize = 4096;

    QByteArray pattern(fileSize, 'A');

    QString fileName = QDir::currentPath() + '/' + "qfile_map_testfile";
    if (QFile::exists(fileName)) {
        QVERIFY(QFile::setPermissions(fileName,
            QFile::WriteOwner | QFile::ReadOwner | QFile::WriteUser | QFile::ReadUser));
        QFile::remove(fileName);
    }
    QFile file(fileName);

    // make a file
    QVERIFY2(file.open(QFile::ReadWrite), msgOpenFailed(file).constData());
    QVERIFY(file.write(pattern));
    QVERIFY(file.flush());
    file.close();

    // open according to our mode
    const QIODevice::OpenMode om(openMode);
    QVERIFY2(file.open(om), msgOpenFailed(om, file).constData());

    uchar *memory = file.map(0, fileSize, QFileDevice::MemoryMapFlags(flags));
    QVERIFY(memory);
    QVERIFY(memcmp(memory, pattern, fileSize) == 0);

    if ((openMode & QIODevice::WriteOnly) || (flags & QFileDevice::MapPrivateOption)) {
        // try to write to the file
        *memory = 'a';
        file.unmap(memory);
        file.close();
        QVERIFY(file.open(QIODevice::OpenMode(openMode)));
        file.seek(0);
        char c;
        QVERIFY(file.getChar(&c));
        QCOMPARE(c, (flags & QFileDevice::MapPrivateOption) ? 'A' : 'a');
    }

    file.close();
}

void tst_QFile::mapWrittenFile_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("buffered") << 0;
    QTest::newRow("unbuffered") << int(QIODevice::Unbuffered);
}

void tst_QFile::mapWrittenFile()
{
    static const char data[128] = "Some data padded with nulls\n";
    QFETCH(int, mode);

    QString fileName = QDir::currentPath() + '/' + "qfile_map_testfile";

    if (QFile::exists(fileName)) {
        QVERIFY(QFile::setPermissions(fileName,
            QFile::WriteOwner | QFile::ReadOwner | QFile::WriteUser | QFile::ReadUser));
        QFile::remove(fileName);
    }
    QFile file(fileName);
    const QIODevice::OpenMode om = QIODevice::ReadWrite | QIODevice::OpenMode(mode);
    QVERIFY2(file.open(om), msgOpenFailed(om, file).constData());
    QCOMPARE(file.write(data, sizeof data), qint64(sizeof data));
    if ((mode & QIODevice::Unbuffered) == 0)
        file.flush();

    // test that we can read the data we've just written, without closing the file
    uchar *memory = file.map(0, sizeof data);
    QVERIFY(memory);
    QVERIFY(memcmp(memory, data, sizeof data) == 0);

    file.close();
    file.remove();
}

void tst_QFile::openDirectory()
{
    QFile f1(m_resourcesDir);
    // it's a directory, it must exist
    QVERIFY(f1.exists());

    // ...but not be openable
    QVERIFY(!f1.open(QIODevice::ReadOnly));
    f1.close();
    QVERIFY(!f1.open(QIODevice::ReadOnly|QIODevice::Unbuffered));
    f1.close();
    QVERIFY(!f1.open(QIODevice::ReadWrite));
    f1.close();
    QVERIFY(!f1.open(QIODevice::WriteOnly));
    f1.close();
    QVERIFY(!f1.open(QIODevice::WriteOnly|QIODevice::Unbuffered));
    f1.close();
}

static qint64 streamExpectedSize(int fd)
{
    QT_STATBUF sb;
    if (QT_FSTAT(fd, &sb) != -1)
        return sb.st_size;
    qErrnoWarning("Could not fstat fd %d", fd);
    return 0;
}

static qint64 streamCurrentPosition(int fd)
{
    QT_STATBUF sb;
    if (QT_FSTAT(fd, &sb) != -1) {
        QT_OFF_T pos = -1;
        if ((sb.st_mode & QT_STAT_MASK) == QT_STAT_REG)
            pos = QT_LSEEK(fd, 0, SEEK_CUR);
        if (pos != -1)
            return pos;
        // failure to lseek() is not a problem
    } else {
        qErrnoWarning("Could not fstat fd %d", fd);
    }
    return 0;
}

static qint64 streamCurrentPosition(FILE *f)
{
    QT_STATBUF sb;
    if (QT_FSTAT(QT_FILENO(f), &sb) != -1) {
        QT_OFF_T pos = -1;
        if ((sb.st_mode & QT_STAT_MASK) == QT_STAT_REG)
            pos = QT_FTELL(f);
        if (pos != -1)
            return pos;
        // failure to ftell() is not a problem
    } else {
        qErrnoWarning("Could not fstat fd %d", QT_FILENO(f));
    }
    return 0;
}

class MessageHandler {
public:
    MessageHandler(QtMessageHandler messageHandler = handler)
    {
        ok = true;
        oldMessageHandler = qInstallMessageHandler(messageHandler);
    }

    ~MessageHandler()
    {
        qInstallMessageHandler(oldMessageHandler);
    }

    static bool testPassed()
    {
        return ok;
    }
protected:
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
    {
        if (msg == QString::fromLatin1("QIODevice::seek: Cannot call seek on a sequential device"))
            ok = false;
        // Defer to old message handler.
        if (oldMessageHandler)
            oldMessageHandler(type, context, msg);
    }

    static QtMessageHandler oldMessageHandler;
    static bool ok;
};

bool MessageHandler::ok = true;
QtMessageHandler MessageHandler::oldMessageHandler = 0;

void tst_QFile::openStandardStreamsFileDescriptors()
{

    // Check that QIODevice::seek() isn't called when opening a sequential device (QFile).
    MessageHandler msgHandler;

    {
        QFile in;
        QVERIFY(in.open(STDIN_FILENO, QIODevice::ReadOnly));
        QCOMPARE( in.pos(), streamCurrentPosition(STDIN_FILENO) );
        QCOMPARE( in.size(), streamExpectedSize(STDIN_FILENO) );
    }

    {
        QFile out;
        QVERIFY(out.open(STDOUT_FILENO, QIODevice::WriteOnly));
        QCOMPARE( out.pos(), streamCurrentPosition(STDOUT_FILENO) );
        QCOMPARE( out.size(), streamExpectedSize(STDOUT_FILENO) );
    }

    {
        QFile err;
        QVERIFY(err.open(STDERR_FILENO, QIODevice::WriteOnly));
        QCOMPARE( err.pos(), streamCurrentPosition(STDERR_FILENO) );
        QCOMPARE( err.size(), streamExpectedSize(STDERR_FILENO) );
    }

    QVERIFY(msgHandler.testPassed());
}

void tst_QFile::openStandardStreamsBufferedStreams()
{
    // Check that QIODevice::seek() isn't called when opening a sequential device (QFile).
    MessageHandler msgHandler;

    // Using streams
    {
        QFile in;
        QVERIFY(in.open(stdin, QIODevice::ReadOnly));
        QCOMPARE( in.pos(), streamCurrentPosition(stdin) );
        QCOMPARE( in.size(), streamExpectedSize(QT_FILENO(stdin)) );
    }

    {
        QFile out;
        QVERIFY(out.open(stdout, QIODevice::WriteOnly));
        QCOMPARE( out.pos(), streamCurrentPosition(stdout) );
        QCOMPARE( out.size(), streamExpectedSize(QT_FILENO(stdout)) );
    }

    {
        QFile err;
        QVERIFY(err.open(stderr, QIODevice::WriteOnly));
        QCOMPARE( err.pos(), streamCurrentPosition(stderr) );
        QCOMPARE( err.size(), streamExpectedSize(QT_FILENO(stderr)) );
    }

    QVERIFY(msgHandler.testPassed());
}

void tst_QFile::writeNothing()
{
    for (int i = 0; i < NumberOfFileTypes; ++i) {
        QFile file("file.txt");
        QVERIFY( openFile(file, QIODevice::WriteOnly | QIODevice::Unbuffered, FileType(i)) );
        QVERIFY( 0 == file.write((char *)0, 0) );
        QCOMPARE( file.error(), QFile::NoError );
        closeFile(file);
    }
}

void tst_QFile::resize_data()
{
    QTest::addColumn<int>("filetype");

    QTest::newRow("native") << int(OpenQFile);
    QTest::newRow("fileno") << int(OpenFd);
    QTest::newRow("stream") << int(OpenStream);
}

void tst_QFile::resize()
{
    QFETCH(int, filetype);
    QString filename(QLatin1String("file.txt"));
    QFile file(filename);
    QVERIFY(openFile(file, QIODevice::ReadWrite, FileType(filetype)));
    QVERIFY(file.resize(8));
    QCOMPARE(file.size(), qint64(8));
    closeFile(file);
    QFile::resize(filename, 4);
    QCOMPARE(QFileInfo(filename).size(), qint64(4));
}

void tst_QFile::objectConstructors()
{
    QObject ob;
    QFile* file1 = new QFile(m_testFile, &ob);
    QFile* file2 = new QFile(&ob);
    QVERIFY(file1->exists());
    QVERIFY(!file2->exists());
}

void tst_QFile::caseSensitivity()
{
#if defined(Q_OS_WIN)
    const bool caseSensitive = false;
#elif defined(Q_OS_DARWIN)
     const bool caseSensitive = pathconf(QDir::currentPath().toLatin1().constData(), _PC_CASE_SENSITIVE) == 1;
#else
    const bool caseSensitive = true;
#endif

    QByteArray testData("a little test");
    QString filename("File.txt");
    {
        QFile f(filename);
        QVERIFY2(f.open(QIODevice::WriteOnly), msgOpenFailed(f));
        QVERIFY(f.write(testData));
        f.close();
    }
    QFileInfo fi(filename);
    QVERIFY(fi.exists());
    const auto alternates = {
        u"file.txt"_s,
        u"File.TXT"_s,
        u"fIlE.TxT"_s,
        fi.absoluteFilePath().toUpper(),
        fi.absoluteFilePath().toLower(),
    };
    for (const QString &alt : alternates) {
        QFileInfo fi2(alt);
        QCOMPARE(fi2.exists(), !caseSensitive);
        QCOMPARE(fi.size() == fi2.size(), !caseSensitive);
        QFile f2(alt);
        QCOMPARE(f2.open(QIODevice::ReadOnly), !caseSensitive);
        if (!caseSensitive)
            QCOMPARE(f2.readAll(), testData);
    }
}

//MSVCRT asserts when any function is called with a closed file handle.
//This replaces the default crashing error handler with one that ignores the error (allowing EBADF to be returned)
class AutoIgnoreInvalidParameter
{
public:
#if defined(Q_OS_WIN) && defined (Q_CC_MSVC)
    static void ignore_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {}
    AutoIgnoreInvalidParameter()
    {
        oldHandler = _set_invalid_parameter_handler(ignore_invalid_parameter);
        //also disable the abort/retry/ignore popup
        oldReportMode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    }
    ~AutoIgnoreInvalidParameter()
    {
        //restore previous settings
        _set_invalid_parameter_handler(oldHandler);
        _CrtSetReportMode(_CRT_ASSERT, oldReportMode);
    }
    _invalid_parameter_handler oldHandler;
    int oldReportMode;
#endif
};

void tst_QFile::autocloseHandle()
{
    {
        QFile file("readonlyfile");
        QVERIFY(openFile(file, QIODevice::ReadOnly, OpenFd, QFile::AutoCloseHandle));
        int fd = fd_;
        QCOMPARE(file.handle(), fd);
        file.close();
        fd_ = -1;
        QCOMPARE(file.handle(), -1);
        AutoIgnoreInvalidParameter a;
        Q_UNUSED(a);
        //file is closed, read should fail
        char buf;
        QCOMPARE((int)QT_READ(fd, &buf, 1), -1);
        QVERIFY(errno == EBADF);
    }

    {
        QFile file("readonlyfile");
        QVERIFY(openFile(file, QIODevice::ReadOnly, OpenFd, QFile::DontCloseHandle));
        QCOMPARE(file.handle(), fd_);
        file.close();
        QCOMPARE(file.handle(), -1);
        //file is not closed, read should succeed
        char buf;
        QCOMPARE((int)QT_READ(fd_, &buf, 1), 1);
        QT_CLOSE(fd_);
        fd_ = -1;
    }

    {
        QFile file("readonlyfile");
        QVERIFY(openFile(file, QIODevice::ReadOnly, OpenStream, QFile::AutoCloseHandle));
        int fd = QT_FILENO(stream_);
        QCOMPARE(file.handle(), fd);
        file.close();
        stream_ = 0;
        QCOMPARE(file.handle(), -1);
        AutoIgnoreInvalidParameter a;
        Q_UNUSED(a);
        //file is closed, read should fail
        char buf;
        QCOMPARE((int)QT_READ(fd, &buf, 1), -1); //not using fread because the FILE* was freed by fclose
    }

    {
        QFile file("readonlyfile");
        QVERIFY(openFile(file, QIODevice::ReadOnly, OpenStream, QFile::DontCloseHandle));
        QCOMPARE(file.handle(), int(QT_FILENO(stream_)));
        file.close();
        QCOMPARE(file.handle(), -1);
        //file is not closed, read should succeed
        char buf;
        QCOMPARE(int(::fread(&buf, 1, 1, stream_)), 1);
        ::fclose(stream_);
        stream_ = 0;
    }
}

void tst_QFile::reuseQFile()
{
    // QTemporaryDir is current dir, no need to remove these files
    const QString filename1("filegt16k");
    const QString filename2("file16k");

    // create test files for reusing QFile object
    QFile file;
    file.setFileName(filename1);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray ba(17408, 'a');
    qint64 written = file.write(ba);
    QCOMPARE(written, 17408);
    file.close();

    file.setFileName(filename2);
    QVERIFY(file.open(QIODevice::WriteOnly));
    ba.resize(16384);
    written = file.write(ba);
    QCOMPARE(written, 16384);
    file.close();

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.size(), 16384);
    QCOMPARE(file.pos(), qint64(0));
    QVERIFY(file.seek(10));
    QCOMPARE(file.pos(), qint64(10));
    QVERIFY(file.seek(0));
    QCOMPARE(file.pos(), qint64(0));
    QCOMPARE(file.readAll(), ba);
    file.close();

    file.setFileName(filename1);
    QVERIFY(file.open(QIODevice::ReadOnly));

    // read first file
    {
        // get file size without touching QFile
        QFileInfo fi(filename1);
        const qint64 fileSize = fi.size();
        file.read(fileSize);
        QVERIFY(file.atEnd());
        file.close();
    }

    // try again with the next file with the same QFile object
    file.setFileName(filename2);
    QVERIFY(file.open(QIODevice::ReadOnly));

    // read second file
    {
        // get file size without touching QFile
        QFileInfo fi(filename2);
        const qint64 fileSize = fi.size();
        file.read(fileSize);
        QVERIFY(file.atEnd());
        file.close();
    }
}

void tst_QFile::supportsMoveToTrash()
{
    // enforce the result according to our current implementation details
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // Windows and macOS: definitely supported
    QVERIFY(QFile::supportsMoveToTrash());
#elif defined(Q_OS_DARWIN)
    // Other Darwin platforms: not supported
    // (though Apple docs say trashItemAtURL is supported)
    QVERIFY(!QFile::supportsMoveToTrash());
#elif defined(Q_OS_ANDROID)
    // Android: not supported (we get EACCES even for $HOME files)
    QVERIFY(!QFile::supportsMoveToTrash());
#elif !defined(AT_FDCWD)
    // Unix platforms without the POSIX atfile support: not supported
    QVERIFY(!QFile::supportsMoveToTrash());
#else
    QVERIFY(QFile::supportsMoveToTrash());
#endif
}

void tst_QFile::moveToTrash_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("create");
    QTest::addColumn<bool>("result");

    // success cases
    {
        QTemporaryFile temp(QDir::tempPath() + "/tst_qfile-moveToTrash-XXXXXX");
        if (!temp.open())
            QSKIP("Failed to create temporary file!");
        QTest::newRow("temporary file") << temp.fileName() << true << true;
#if defined(Q_OS_UNIX) && !defined(Q_OS_WASM)
        if (QDir::tempPath() == "/tmp")
            QTest::newRow("var-temporary file") << "/var" + temp.fileName() << true << true;
#endif
    }
    {
        QTemporaryDir tempDir(QDir::tempPath() + "/tst_qfile-moveToTrash-XXXXXX");
        if (!tempDir.isValid())
            QSKIP("Failed to create temporary directory!");
        tempDir.setAutoRemove(false);
        QTest::newRow("temporary dir")
            << tempDir.path() + QLatin1Char('/')
            << true << true;
#if defined(Q_OS_UNIX) && !defined(Q_OS_WASM)
        if (QDir::tempPath() == "/tmp")
            QTest::newRow("var-temporary dir") << "/var" + tempDir.path() << true << true;
#endif
    }
    {
        QTemporaryDir homeDir(QDir::homePath() + QLatin1String("/tst_qfile.moveToTrash-XXXXXX"));
        if (!homeDir.isValid())
            QSKIP("Failed to create temporary directory in $HOME!");
        QTemporaryFile homeFile(homeDir.path()
                              + QLatin1String("/tst_qfile-moveToTrash-XXXXX"));
        if (!homeFile.open())
            QSKIP("Failed to create temporary file in $HOME");
        homeDir.setAutoRemove(false);
        QTest::newRow("home file")
            << homeFile.fileName()
            << true << true;

        QTest::newRow("home dir")
            << homeDir.path() + QLatin1Char('/')
            << true << true;
    }
    QTest::newRow("relative") << QStringLiteral("tst_qfile-moveToTrash.tmp") << true << true;

    // failure cases
    QTest::newRow("root") << QDir::rootPath() << false << false;
    QTest::newRow("no-such-file") << QString::fromLatin1("no/such/file") << false << false;
}

void tst_QFile::moveToTrash()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

    QFETCH(QString, source);
    QFETCH(bool, create);
    QFETCH(bool, result);

    auto ensureFile = [](const QString &source, bool create) {
        if (QFileInfo::exists(source) || !create)
            return;
        if (source.endsWith(QLatin1Char('/'))) {
            QDir::root().mkdir(source);
            QFile file(source + QLatin1String("test"));
            if (!file.open(QIODevice::WriteOnly))
                QSKIP("Couldn't create directory with file");
        } else {
            QFile sourceFile(source);
            QVERIFY2(sourceFile.open(QFile::WriteOnly | QFile::Text), qPrintable(sourceFile.errorString()));
            sourceFile.close();
        }
    };
    auto cleanupFile = [source, create]() {
        if (!QFileInfo::exists(source) || !create)
            return;
        if (source.endsWith(QLatin1Char('/'))) {
            QDir(source).removeRecursively();
        } else {
            QFile sourceFile(source);
            sourceFile.remove();
        }
    };

    ensureFile(source, create);
    if (!QFileInfo::exists(source) && create) return;

    /* This test makes assumptions about the file system layout
       which might be wrong - moveToTrash may fail if the file lives
       on a file system that is different from the home file system, and
       has no .Trash directory.
    */
    const QStorageInfo sourceStorage(source);
    const bool mayFail = sourceStorage.isValid()
                      && QStorageInfo(source) != QStorageInfo(QDir::home());

    // non-static version
    {
        QFile sourceFile(source);
        const bool success = sourceFile.moveToTrash();

        // tolerate moveToTrash failing
        if (result && !success && mayFail)
            result = false;

        if (result) {
            // if any of the test fails, we still want to remove the file
            auto onFailure = qScopeGuard(cleanupFile);
            QVERIFY2(success, qPrintable(sourceFile.errorString()));
            QCOMPARE(sourceFile.error(), QFile::NoError);
            QVERIFY(source != sourceFile.fileName());
            if (!sourceFile.fileName().isEmpty()) {
                QVERIFY2(sourceFile.exists(), qPrintable(sourceFile.fileName()));
                // remove file/dir in trash as well, don't fill disk
                if (source.endsWith(QLatin1Char('/')))
                    QDir(sourceFile.fileName()).removeRecursively();
                else
                    sourceFile.remove();
            }
        } else {
            QVERIFY(!success);
            QVERIFY(!sourceFile.errorString().isEmpty());
            QCOMPARE(source, sourceFile.fileName());
        }
    }

    // don't retry
    if (mayFail)
        return;

    // static version
    {
        ensureFile(source, create);
        if (!QFileInfo::exists(source) && create) return;
        QString pathInTrash;
        const bool success = QFile::moveToTrash(source, &pathInTrash);
        QCOMPARE(success, result);
        if (result) {
            auto onFailure = qScopeGuard(cleanupFile);
            QVERIFY(source != pathInTrash);
            if (!pathInTrash.isEmpty()) {
                // remove file/dir in trash as well, don't fill disk
                QVERIFY2(QFile::exists(pathInTrash), qPrintable(pathInTrash));
                if (source.endsWith(QLatin1Char('/')))
                    QDir(pathInTrash).removeRecursively();
                else
                    QFile::remove(pathInTrash);
            }
        }
    }
}

void tst_QFile::moveToTrashDuplicateName()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

    QString origFileName = []() {
        QTemporaryFile temp(QDir::homePath() + "/tst_qfile.moveToTrashOpenFile.XXXXXX");
        temp.setAutoRemove(false);
        if (!temp.open())
            qWarning("Failed to create temporary file: %ls", qUtf16Printable(temp.errorString()));
        return temp.fileName();
    }();

    QFile f1(origFileName);
    QFile f2(origFileName);
    [&] {
        QByteArrayView message1 = "Hello, World\n";
        QVERIFY2(f1.open(QIODevice::ReadWrite | QIODevice::Unbuffered), qPrintable(f1.errorString()));
        f1.write(message1.data(), message1.size());
        QVERIFY2(f1.moveToTrash(), qPrintable(f1.errorString()));

        QByteArrayView message2 = "Good morning, Vietnam!\n";
        QVERIFY2(f2.open(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::NewOnly),
                 qPrintable(f2.errorString()));
        f2.write(message2.data(), message2.size());
        QVERIFY2(f2.moveToTrash(), qPrintable(f2.errorString()));

        QCOMPARE_NE(f1.fileName(), f2.fileName());
    }();
    f1.remove();
    if (!f2.fileName().isEmpty())
        f2.remove();
    QFile::remove(origFileName);
}

void tst_QFile::moveToTrashOpenFile_data()
{
    QTest::addColumn<bool>("useStatic");
    QTest::addColumn<bool>("success");

    // QFile::moveToTrash() non-static member closes the file before trashing,
    // so this must always succeed.
    QTest::newRow("member") << false << true;

    // QFile::moveToTrash() static member cannot close the file because it
    // operates on another QFile, so this operation will fail on OSes that do
    // not permit deleting open files.
    QTest::newRow("static") << true
#ifdef Q_OS_WIN
                            << false;
#else
                            << true;
#endif
}

void tst_QFile::moveToTrashOpenFile()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

    QFETCH(bool, useStatic);
    QFETCH(bool, success);
    const QByteArrayView contents = "Hello, World\n";

    QString newFileName, origFileName;
    auto cleanup = qScopeGuard([&] {
        if (!origFileName.isEmpty())
            QFile::remove(origFileName);
        if (!newFileName.isEmpty() && newFileName != origFileName)
            QFile::remove(newFileName);
    });

    origFileName = []() {
        QTemporaryFile temp(QDir::homePath() + "/tst_qfile.moveToTrashOpenFile.XXXXXX");
        temp.setAutoRemove(false);
        if (!temp.open())
            qWarning("Failed to create temporary file: %ls", qUtf16Printable(temp.errorString()));
        return temp.fileName();
    }();

    QFile f;
    f.setFileName(origFileName);
    QVERIFY2(f.open(QIODevice::ReadWrite | QIODevice::Unbuffered), qPrintable(f.errorString()));
    f.write(contents.data(), contents.size());

    QString errorString;
    auto doMoveToTrash = [&](QFile *f) {
        if (!f->moveToTrash())
            errorString = f->errorString();
        newFileName = f->fileName();
    };
    if (useStatic) {
        // it's the same as the static QFile::moveToTrash(), but gives us
        // the error string
        QFile other(origFileName);
        doMoveToTrash(&other);
    } else {
        doMoveToTrash(&f);
    }
    QCOMPARE_NE(f.fileName(), QString());

    if (success) {
        QCOMPARE(errorString, QString());
        QCOMPARE_NE(newFileName, origFileName);         // must have changed!
        QVERIFY(!QFile::exists(origFileName));
        QVERIFY(QFile::exists(newFileName));
        QCOMPARE(QFileInfo(newFileName).size(), contents.size());
    } else {
        QCOMPARE_NE(errorString, QString());
        QCOMPARE(newFileName, origFileName);            // mustn't have changed!
        QVERIFY(QFile::exists(origFileName));
        QCOMPARE(QFileInfo(origFileName).size(), contents.size());
    }
}

void tst_QFile::moveToTrashSymlinkToFile()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

    QTemporaryFile temp(QDir::homePath() + "/tst_qfile.moveToTrashSymlinkFile.XXXXXX");
    QVERIFY2(temp.open(), "Failed to create temporary file: " + temp.errorString().toLocal8Bit());

    // Create the symlink
    const QString linkName = temp.fileName() + ".lnk";
    QVERIFY2(temp.link(linkName), "Failed to create link: " + temp.errorString().toLocal8Bit());
    auto cleanLink = qScopeGuard([&]() {
        QFile::remove(linkName);
    });

    // now trash it
    QFile symlink(linkName);
    QVERIFY(symlink.moveToTrash());
    QCOMPARE_NE(symlink.fileName(), linkName);

    // confirm that the target is still a symlink
    QFileInfo fi(symlink.fileName());
    QVERIFY(fi.isSymLink());
    QVERIFY(fi.isFile());   // we used an absolute path, so it should not be broken!
    symlink.remove();

    // confirm that the symlink disappeared but the original file is still present
    QVERIFY(QFile::exists(temp.fileName()));
    QVERIFY(!QFile::exists(linkName));
    cleanLink.dismiss();
}

void tst_QFile::moveToTrashSymlinkToDirectory_data()
{
    QTest::addColumn<bool>("appendSlash");
    QTest::newRow("without-slash") << false;
    QTest::newRow("with-slash") << true;
}

void tst_QFile::moveToTrashSymlinkToDirectory()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

    QFETCH(bool, appendSlash);
    QTemporaryDir temp(QDir::homePath() + "/tst_qfile.moveToTrashSymlinkDir.XXXXXX");
    QVERIFY2(temp.isValid(), "Failed to create temporary dir: " + temp.errorString().toLocal8Bit());

    // Create the symlink
    const QString linkName = temp.path() + ".lnk";
    QVERIFY(QFile::link(temp.path(), linkName));
    auto cleanLink = qScopeGuard([&]() {
        QFile::remove(linkName);
    });

    // now trash it
    QFile symlink(appendSlash ? linkName + u'/' : linkName);
    QVERIFY(symlink.moveToTrash());
    QCOMPARE_NE(symlink.fileName(), linkName);
    QCOMPARE_NE(symlink.fileName(), linkName + u'/');

    // confirm that the target is still a symlink
    QFileInfo fi(symlink.fileName());
    QVERIFY(fi.isSymLink());
    QVERIFY(fi.isDir());    // we used an absolute path, so it should not be broken!
    symlink.remove();

    // confirm that the symlink disappeared but the original dir is still present
    QVERIFY(QFile::exists(temp.path()));
    QVERIFY(!QFile::exists(linkName));
    cleanLink.dismiss();
}

void tst_QFile::moveToTrashXdgSafety()
{
    if (!QFile::supportsMoveToTrash())
        QSKIP("This platform doesn't implement a trash bin");

#if defined(Q_OS_WIN) || defined(Q_OS_DARWIN) || defined(Q_OS_ANDROID) || defined(Q_OS_WEBOS)
    QSKIP("This test is specific to XDG Unix systems");
#else
    QDir(m_temporaryDir.path()).mkdir("emptydir");

    // See if we can find a writable volume to conduct our tests on
    QString volumeRoot;
    QStorageInfo homeVolume(QDir::homePath());
    auto isVolumeSuitable = [this](const QString &rootPath) {
        return QFile::link(m_temporaryDir.path() + "/emptydir", rootPath + "/.Trash");
    };
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        if (volume.isRoot())
            continue;
        if (volume == homeVolume)
            continue;

        if (isVolumeSuitable(volume.rootPath())) {
            volumeRoot = volume.rootPath();
            break;
        }
    }

#  ifdef Q_OS_LINUX
    // fallback to /dev/shm, which is usually a tmpfs but is ignored by
    // QStorageInfo as a virtual filesystem
    if (volumeRoot.isEmpty() && isVolumeSuitable("/dev/shm"))
        volumeRoot = "/dev/shm";
#  endif

    if (volumeRoot.isEmpty())
        QSKIP("Could not find any suitable volume to run this test with");

    QDir genericTrashDir = volumeRoot + "/.Trash";
    auto cleanup = qScopeGuard([&] {
        if (QFileInfo(genericTrashDir.path()).isDir())
            genericTrashDir.removeRecursively();
        else
            QFile::remove(genericTrashDir.path());
    });

    QString testFileName = volumeRoot + "/tst_qfile.moveToTrashSafety." + QString::number(getpid());
    auto tryTrashing = [&] {
        static int counter = 0;
        QFile f(testFileName + u'.' + QString::number(counter++));
        if (!f.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            qWarning("Failed to create temporary file: %ls", qUtf16Printable(f.errorString()));
            return false;
        }
        bool ok = f.moveToTrash();
        f.remove();
        f.close();
        return ok;
    };

    QTest::ignoreMessage(QtCriticalMsg,
                         "Warning: '" + QFile::encodeName(genericTrashDir.absolutePath())
                         + "' is a symlink to '" + QFile::encodeName(m_temporaryDir.path())
                         + "/emptydir'");
    QVERIFY(tryTrashing());
    QVERIFY(genericTrashDir.entryList(QDir::NoDotAndDotDot).isEmpty());

    QFile::remove(genericTrashDir.path());
    genericTrashDir.mkdir(genericTrashDir.path(), QFile::ExeOwner | QFile::ReadOwner);
    QTest::ignoreMessage(QtCriticalMsg, "Warning: '" + QFile::encodeName(genericTrashDir.absolutePath())
                         + "' doesn't have sticky bit set!");
    QVERIFY(tryTrashing());
    QVERIFY(genericTrashDir.entryList(QDir::NoDotAndDotDot).isEmpty());

    if (geteuid() != 0) {
        // set the sticky bit, but make the dir unwritable; there'll be no
        // warning and we should just fall back to the next option
        chmod(QFile::encodeName(genericTrashDir.path()), 01555);
        QVERIFY(tryTrashing());
        QVERIFY(genericTrashDir.entryList(QDir::NoDotAndDotDot).isEmpty());

        // ditto for our user's subdir now
        chmod(QFile::encodeName(genericTrashDir.path()), 01755);
        genericTrashDir.mkdir(QString::number(getuid()), QFile::ReadOwner);
        QVERIFY(tryTrashing());
    }
#endif
}

void tst_QFile::stdfilesystem()
{
#if QT_CONFIG(cxx17_filesystem)
    namespace fs = std::filesystem;
    auto toFSPath = [](const QFile &file) { return fs::path(file.fileName().toStdU16String()); };
    fs::path path { "./path" };
    QFile file(path);
    QCOMPARE(toFSPath(file), path);

    QCOMPARE(path, file.filesystemFileName());

    {
        QFile parentedFile(path, this);
        QCOMPARE(file.fileName(), parentedFile.fileName());
        QCOMPARE(parentedFile.parent(), this);
    }

    path = path / "filename";
    file.setFileName(path);
    QCOMPARE(toFSPath(file), path);

    path = "test-file";
    file.setFileName(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    path = "tile-fest";
    QVERIFY(file.rename(path));
    QVERIFY(fs::exists(path));
#ifdef Q_OS_WIN
    fs::path linkfile { "test-link.lnk" };
#else
    fs::path linkfile { "test-link" };
#endif
    QVERIFY(file.link(linkfile));
    QVERIFY(fs::exists(linkfile));
    QVERIFY(QFile::remove(linkfile));
    QVERIFY(QFile::link(file.filesystemFileName(), linkfile));
    QVERIFY(fs::exists(linkfile));
    QCOMPARE(QFileInfo(QFile::filesystemSymLinkTarget(linkfile)),
             QFileInfo(file.filesystemFileName()));
    QCOMPARE(QFileInfo(QFile(linkfile).filesystemSymLinkTarget()),
             QFileInfo(file.filesystemFileName()));

    fs::path copyfile { "copy-file" };
    QVERIFY(file.copy(copyfile));
    QVERIFY(fs::exists(copyfile));
    QVERIFY(QFile::remove(copyfile));
    QVERIFY(QFile::copy(file.filesystemFileName(), copyfile));
    QVERIFY(fs::exists(copyfile));

    QFileDevice::Permissions p = QFile::permissions(path);
    QVERIFY(p.testFlag(QFile::WriteUser) || p.testFlag(QFile::WriteOwner)); // some we know for sure
    if (p.testFlag(QFile::ReadUser))
        p.setFlag(QFile::ReadUser, false);
    else if (p.testFlag(QFile::ReadOwner))
        p.setFlag(QFile::ReadOwner, false);
    QVERIFY(QFile::setPermissions(path, p));

    path = "test-exists";
    fs::create_directory(path);
    QVERIFY(QFile::exists(path) == fs::exists(path));
#else
    QSKIP("Not supported");
#endif
}

QTEST_MAIN(tst_QFile)
#include "tst_qfile.moc"
