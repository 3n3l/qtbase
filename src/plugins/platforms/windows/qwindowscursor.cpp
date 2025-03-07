// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef  QT_NO_CURSOR
#include "qwindowscursor.h"
#include "qwindowscontext.h"
#include "qwindowswindow.h"
#include "qwindowsscreen.h"

#include <QtGui/qbitmap.h>
#include <QtGui/qimage.h>
#include <QtGui/qbitmap.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>
#include <QtGui/private/qguiapplication_p.h> // getPixmapCursor()
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/private/qpixmap_win_p.h>
#include <QtCore/private/qwinregistry_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qscopedpointer.h>

static bool initResources()
{
#if QT_CONFIG(imageformat_png)
    Q_INIT_RESOURCE(cursors);
#endif
    return true;
}

QT_BEGIN_NAMESPACE

using namespace Qt::Literals::StringLiterals;

/*!
    \class QWindowsCursorCacheKey
    \brief Cache key for storing values in a QHash with a QCursor as key.

    \internal
*/

QWindowsPixmapCursorCacheKey::QWindowsPixmapCursorCacheKey(const QCursor &c)
    : bitmapCacheKey(c.pixmap().cacheKey()), maskCacheKey(0)
{
    if (!bitmapCacheKey) {
        Q_ASSERT(!c.bitmap().isNull());
        Q_ASSERT(!c.mask().isNull());
        bitmapCacheKey = c.bitmap().cacheKey();
        maskCacheKey = c.mask().cacheKey();
    }
}

/*!
    \class QWindowsCursor
    \brief Platform cursor implementation

    Note that whereas under X11, a cursor can be set as a property of
    a window, there is only a global SetCursor() function on Windows.
    Each Window sets on the global cursor on receiving a Enter-event
    as do the Window manager frames (resize/move handles).

    \internal
    \sa QWindowsWindowCursor
*/

HCURSOR QWindowsCursor::createPixmapCursor(QPixmap pixmap, const QPoint &hotSpot, qreal scaleFactor)
{
    HCURSOR cur = nullptr;
    const qreal pixmapScaleFactor = scaleFactor / pixmap.devicePixelRatio();
    if (!qFuzzyCompare(pixmapScaleFactor, 1)) {
        pixmap = pixmap.scaled((pixmapScaleFactor * QSizeF(pixmap.size())).toSize(),
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QBitmap mask = pixmap.mask();
    if (mask.isNull()) {
        mask = QBitmap(pixmap.size());
        mask.fill(Qt::color1);
    }

    HBITMAP ic = qt_pixmapToWinHBITMAP(pixmap, /* HBitmapAlpha */ 2);
    const HBITMAP im = qt_createIconMask(mask);

    ICONINFO ii;
    ii.fIcon     = 0;
    ii.xHotspot  = DWORD(qRound(hotSpot.x() * scaleFactor));
    ii.yHotspot  = DWORD(qRound(hotSpot.y() * scaleFactor));
    ii.hbmMask   = im;
    ii.hbmColor  = ic;

    cur = CreateIconIndirect(&ii);

    DeleteObject(ic);
    DeleteObject(im);
    return cur;
}

// Create a cursor from image and mask of the format QImage::Format_Mono.
static HCURSOR createBitmapCursor(const QImage &bbits, const QImage &mbits,
                                  QPoint hotSpot = QPoint(-1, -1),
                                  bool invb = false, bool invm = false)
{
    const int width = bbits.width();
    const int height = bbits.height();
    if (hotSpot.x() < 0)
        hotSpot.setX(width / 2);
    if (hotSpot.y() < 0)
        hotSpot.setY(height / 2);
    // a ddb is word aligned, QImage depends on bow it was created
    const auto bplDdb = qMax(1, ((width + 15) >> 4) << 1);
    const auto bplImg = int(bbits.bytesPerLine());
    QScopedArrayPointer<uchar> xBits(new uchar[height * bplDdb]);
    QScopedArrayPointer<uchar> xMask(new uchar[height * bplDdb]);
    int x = 0;
    for (int i = 0; i < height; ++i) {
        const uchar *bits = bbits.constScanLine(i);
        const uchar *mask = mbits.constScanLine(i);
        for (int j = 0; j < bplImg && j < bplDdb; ++j) {
            uchar b = bits[j];
            uchar m = mask[j];
            if (invb)
                b ^= 0xff;
            if (invm)
                m ^= 0xff;
            xBits[x] = ~m;
            xMask[x] = b ^ m;
            ++x;
        }
        for (int i = bplImg; i < bplDdb; ++i) {
            xBits[x] = 0;
            xMask[x] = 0;
            ++x;
        }
    }
    return CreateCursor(GetModuleHandle(nullptr), hotSpot.x(), hotSpot.y(), width, height,
                        xBits.data(), xMask.data());
}

// Create a cursor from image and mask of the format QImage::Format_Mono.
static HCURSOR createBitmapCursor(const QCursor &cursor, qreal scaleFactor = 1)
{
    Q_ASSERT(cursor.shape() == Qt::BitmapCursor && !cursor.bitmap().isNull());
    QImage bbits = cursor.bitmap().toImage();
    QImage mbits = cursor.mask().toImage();
    scaleFactor /= bbits.devicePixelRatio();
    if (!qFuzzyCompare(scaleFactor, 1)) {
        const QSize scaledSize = (QSizeF(bbits.size()) * scaleFactor).toSize();
        bbits = bbits.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        mbits = mbits.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    bbits = std::move(bbits).convertToFormat(QImage::Format_Mono);
    mbits = std::move(mbits).convertToFormat(QImage::Format_Mono);
    const bool invb = bbits.colorCount() > 1 && qGray(bbits.color(0)) < qGray(bbits.color(1));
    const bool invm = mbits.colorCount() > 1 && qGray(mbits.color(0)) < qGray(mbits.color(1));
    return createBitmapCursor(bbits, mbits, cursor.hotSpot(), invb, invm);
}

static QSize systemCursorSize() { return QSize(GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR)); }

static QSize screenCursorSize(const QPlatformScreen *screen = nullptr)
{
    const QSize primaryScreenCursorSize = systemCursorSize();
    if (screen) {
        // Correct the size if the DPI value of the screen differs from
        // that of the primary screen.
        if (const QScreen *primaryQScreen = QGuiApplication::primaryScreen()) {
            const QPlatformScreen *primaryScreen = primaryQScreen->handle();
            if (screen != primaryScreen) {
                const qreal logicalDpi = screen->logicalDpi().first;
                const qreal primaryScreenLogicalDpi = primaryScreen->logicalDpi().first;
                if (!qFuzzyCompare(logicalDpi, primaryScreenLogicalDpi))
                    return (QSizeF(primaryScreenCursorSize) * logicalDpi / primaryScreenLogicalDpi).toSize();
            }
        }
    }
    return primaryScreenCursorSize;
}

#if !QT_CONFIG(imageformat_png)

static inline QSize standardCursorSize() { return QSize(32, 32); }

// Create pixmap cursors from data and scale the image if the cursor size is
// higher than the standard 32. Note that bitmap cursors as produced by
// createBitmapCursor() only work for standard sizes (32,48,64...), which does
// not work when scaling the 16x16 openhand cursor bitmaps to 150% (resulting
// in a non-standard 24x24 size).
static QWindowsCursor::PixmapCursor createPixmapCursorFromData(const QSize &screenCursorSize,
                                          // The cursor size the bitmap is targeted for
                                          const QSize &bitmapTargetCursorSize,
                                          // The actual size of the bitmap data
                                          int bitmapSize, const uchar *bits,
                                          const uchar *maskBits)
{
    QPixmap rawImage = QPixmap::fromImage(QBitmap::fromData(QSize(bitmapSize, bitmapSize), bits).toImage());
    rawImage.setMask(QBitmap::fromData(QSize(bitmapSize, bitmapSize), maskBits));

    const qreal factor = qreal(screenCursorSize.width()) / qreal(bitmapTargetCursorSize.width());
    // Scale images if the cursor size is significantly different, starting with 150% where the system cursor
    // size is 48.
    if (qAbs(factor - 1.0) > 0.4) {
        const QTransform transform = QTransform::fromScale(factor, factor);
        rawImage = rawImage.transformed(transform, Qt::SmoothTransformation);
    }
    const QPoint hotSpot(rawImage.width() / 2, rawImage.height() / 2);
    return QWindowsCursor::PixmapCursor(rawImage, hotSpot);
}

QWindowsCursor::PixmapCursor QWindowsCursor::customCursor(Qt::CursorShape cursorShape,
                                                          const QPlatformScreen *screen)
{
    // Non-standard Windows cursors are created from bitmaps
    static const uchar vsplit_bits[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xe0, 0x03, 0x00,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
        0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00,
        0x00, 0xc0, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uchar vsplitm_bits[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
        0x00, 0xc0, 0x01, 0x00, 0x00, 0xe0, 0x03, 0x00, 0x00, 0xf0, 0x07, 0x00,
        0x00, 0xf8, 0x0f, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00,
        0x00, 0xc0, 0x01, 0x00, 0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00,
        0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00,
        0x80, 0xff, 0xff, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00,
        0x00, 0xc0, 0x01, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x00, 0xf0, 0x07, 0x00,
        0x00, 0xe0, 0x03, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uchar hsplit_bits[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
        0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
        0x00, 0x41, 0x82, 0x00, 0x80, 0x41, 0x82, 0x01, 0xc0, 0x7f, 0xfe, 0x03,
        0x80, 0x41, 0x82, 0x01, 0x00, 0x41, 0x82, 0x00, 0x00, 0x40, 0x02, 0x00,
        0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
        0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uchar hsplitm_bits[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00,
        0x00, 0xe0, 0x07, 0x00, 0x00, 0xe2, 0x47, 0x00, 0x00, 0xe3, 0xc7, 0x00,
        0x80, 0xe3, 0xc7, 0x01, 0xc0, 0xff, 0xff, 0x03, 0xe0, 0xff, 0xff, 0x07,
        0xc0, 0xff, 0xff, 0x03, 0x80, 0xe3, 0xc7, 0x01, 0x00, 0xe3, 0xc7, 0x00,
        0x00, 0xe2, 0x47, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00,
        0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
   static const uchar openhand_bits[] = {
        0x80,0x01,0x58,0x0e,0x64,0x12,0x64,0x52,0x48,0xb2,0x48,0x92,
        0x16,0x90,0x19,0x80,0x11,0x40,0x02,0x40,0x04,0x40,0x04,0x20,
        0x08,0x20,0x10,0x10,0x20,0x10,0x00,0x00};
    static const uchar openhandm_bits[] = {
       0x80,0x01,0xd8,0x0f,0xfc,0x1f,0xfc,0x5f,0xf8,0xff,0xf8,0xff,
       0xf6,0xff,0xff,0xff,0xff,0x7f,0xfe,0x7f,0xfc,0x7f,0xfc,0x3f,
       0xf8,0x3f,0xf0,0x1f,0xe0,0x1f,0x00,0x00};
    static const uchar closedhand_bits[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0x48,0x32,0x08,0x50,
        0x10,0x40,0x18,0x40,0x04,0x40,0x04,0x20,0x08,0x20,0x10,0x10,
        0x20,0x10,0x20,0x10,0x00,0x00,0x00,0x00};
    static const uchar closedhandm_bits[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0xf8,0x3f,0xf8,0x7f,
        0xf0,0x7f,0xf8,0x7f,0xfc,0x7f,0xfc,0x3f,0xf8,0x3f,0xf0,0x1f,
        0xe0,0x1f,0xe0,0x1f,0x00,0x00,0x00,0x00};

    static const char * const moveDragCursorXpmC[] = {
    "11 20 3 1",
    ".        c None",
    "a        c #FFFFFF",
    "X        c #000000", // X11 cursor is traditionally black
    "aa.........",
    "aXa........",
    "aXXa.......",
    "aXXXa......",
    "aXXXXa.....",
    "aXXXXXa....",
    "aXXXXXXa...",
    "aXXXXXXXa..",
    "aXXXXXXXXa.",
    "aXXXXXXXXXa",
    "aXXXXXXaaaa",
    "aXXXaXXa...",
    "aXXaaXXa...",
    "aXa..aXXa..",
    "aa...aXXa..",
    "a.....aXXa.",
    "......aXXa.",
    ".......aXXa",
    ".......aXXa",
    "........aa."};

    static const char * const copyDragCursorXpmC[] = {
    "24 30 3 1",
    ".        c None",
    "a        c #000000",
    "X        c #FFFFFF",
    "XX......................",
    "XaX.....................",
    "XaaX....................",
    "XaaaX...................",
    "XaaaaX..................",
    "XaaaaaX.................",
    "XaaaaaaX................",
    "XaaaaaaaX...............",
    "XaaaaaaaaX..............",
    "XaaaaaaaaaX.............",
    "XaaaaaaXXXX.............",
    "XaaaXaaX................",
    "XaaXXaaX................",
    "XaX..XaaX...............",
    "XX...XaaX...............",
    "X.....XaaX..............",
    "......XaaX..............",
    ".......XaaX.............",
    ".......XaaX.............",
    "........XX...aaaaaaaaaaa",
    ".............aXXXXXXXXXa",
    ".............aXXXXXXXXXa",
    ".............aXXXXaXXXXa",
    ".............aXXXXaXXXXa",
    ".............aXXaaaaaXXa",
    ".............aXXXXaXXXXa",
    ".............aXXXXaXXXXa",
    ".............aXXXXXXXXXa",
    ".............aXXXXXXXXXa",
    ".............aaaaaaaaaaa"};

    static const char * const linkDragCursorXpmC[] = {
    "24 30 3 1",
    ".        c None",
    "a        c #000000",
    "X        c #FFFFFF",
    "XX......................",
    "XaX.....................",
    "XaaX....................",
    "XaaaX...................",
    "XaaaaX..................",
    "XaaaaaX.................",
    "XaaaaaaX................",
    "XaaaaaaaX...............",
    "XaaaaaaaaX..............",
    "XaaaaaaaaaX.............",
    "XaaaaaaXXXX.............",
    "XaaaXaaX................",
    "XaaXXaaX................",
    "XaX..XaaX...............",
    "XX...XaaX...............",
    "X.....XaaX..............",
    "......XaaX..............",
    ".......XaaX.............",
    ".......XaaX.............",
    "........XX...aaaaaaaaaaa",
    ".............aXXXXXXXXXa",
    ".............aXXXaaaaXXa",
    ".............aXXXXaaaXXa",
    ".............aXXXaaaaXXa",
    ".............aXXaaaXaXXa",
    ".............aXXaaXXXXXa",
    ".............aXXaXXXXXXa",
    ".............aXXXaXXXXXa",
    ".............aXXXXXXXXXa",
    ".............aaaaaaaaaaa"};

    switch (cursorShape) {
    case Qt::SplitVCursor:
        return createPixmapCursorFromData(screenCursorSize(screen), standardCursorSize(), 32, vsplit_bits, vsplitm_bits);
    case Qt::SplitHCursor:
        return createPixmapCursorFromData(screenCursorSize(screen), standardCursorSize(), 32, hsplit_bits, hsplitm_bits);
    case Qt::OpenHandCursor:
        return createPixmapCursorFromData(screenCursorSize(screen), standardCursorSize(), 16, openhand_bits, openhandm_bits);
    case Qt::ClosedHandCursor:
        return createPixmapCursorFromData(screenCursorSize(screen), standardCursorSize(), 16, closedhand_bits, closedhandm_bits);
    case Qt::DragCopyCursor:
        return QWindowsCursor::PixmapCursor(QPixmap(copyDragCursorXpmC), QPoint(0, 0));
    case Qt::DragMoveCursor:
        return QWindowsCursor::PixmapCursor(QPixmap(moveDragCursorXpmC), QPoint(0, 0));
    case Qt::DragLinkCursor:
        return QWindowsCursor::PixmapCursor(QPixmap(linkDragCursorXpmC), QPoint(0, 0));
    }

    return QWindowsCursor::PixmapCursor();
}
#else // QT_NO_IMAGEFORMAT_PNG
struct QWindowsCustomPngCursor {
    Qt::CursorShape shape;
    int size;
    const char *fileName;
    int hotSpotX;
    int hotSpotY;
};

QWindowsCursor::PixmapCursor QWindowsCursor::customCursor(Qt::CursorShape cursorShape, const QPlatformScreen *screen)
{
    static const QWindowsCustomPngCursor pngCursors[] = {
        { Qt::SplitVCursor, 32, "splitvcursor_32.png", 11, 11 },
        { Qt::SplitVCursor, 48, "splitvcursor_48.png", 16, 17 },
        { Qt::SplitVCursor, 64, "splitvcursor_64.png", 22, 22 },
        { Qt::SplitHCursor, 32, "splithcursor_32.png", 11, 11 },
        { Qt::SplitHCursor, 48, "splithcursor_48.png", 16, 17 },
        { Qt::SplitHCursor, 64, "splithcursor_64.png", 22, 22 },
        { Qt::OpenHandCursor, 32, "openhandcursor_32.png", 10, 12 },
        { Qt::OpenHandCursor, 48, "openhandcursor_48.png", 15, 16 },
        { Qt::OpenHandCursor, 64, "openhandcursor_64.png", 20, 24 },
        { Qt::ClosedHandCursor, 32, "closedhandcursor_32.png", 10, 12 },
        { Qt::ClosedHandCursor, 48, "closedhandcursor_48.png", 15, 16 },
        { Qt::ClosedHandCursor, 64, "closedhandcursor_64.png", 20, 24 },
        { Qt::DragCopyCursor, 32, "dragcopycursor_32.png", 0, 0 },
        { Qt::DragCopyCursor, 48, "dragcopycursor_48.png", 0, 0 },
        { Qt::DragCopyCursor, 64, "dragcopycursor_64.png", 0, 0 },
        { Qt::DragMoveCursor, 32, "dragmovecursor_32.png", 0, 0 },
        { Qt::DragMoveCursor, 48, "dragmovecursor_48.png", 0, 0 },
        { Qt::DragMoveCursor, 64, "dragmovecursor_64.png", 0, 0 },
        { Qt::DragLinkCursor, 32, "draglinkcursor_32.png", 0, 0 },
        { Qt::DragLinkCursor, 48, "draglinkcursor_48.png", 0, 0 },
        { Qt::DragLinkCursor, 64, "draglinkcursor_64.png", 0, 0 }
    };

    const QSize cursorSize = screenCursorSize(screen);
    const QWindowsCustomPngCursor *sEnd = pngCursors + sizeof(pngCursors) / sizeof(pngCursors[0]);
    const QWindowsCustomPngCursor *bestFit = nullptr;
    int sizeDelta = INT_MAX;
    for (const QWindowsCustomPngCursor *s = pngCursors; s < sEnd; ++s) {
        if (s->shape != cursorShape)
            continue;
        const int currentSizeDelta = qMax(s->size, cursorSize.width()) - qMin(s->size, cursorSize.width());
        if (currentSizeDelta < sizeDelta) {
            bestFit = s;
            if (currentSizeDelta == 0)
                break; // Perfect match found
            sizeDelta = currentSizeDelta;
        }
    }

    if (!bestFit)
        return PixmapCursor();

    const QPixmap rawImage(":/qt-project.org/windows/cursors/images/"_L1 +
                           QLatin1StringView(bestFit->fileName));
    return PixmapCursor(rawImage, QPoint(bestFit->hotSpotX, bestFit->hotSpotY));
}
#endif // !QT_NO_IMAGEFORMAT_PNG

struct QWindowsStandardCursorMapping {
    Qt::CursorShape shape;
    LPCWSTR resource;
};

HCURSOR QWindowsCursor::createCursorFromShape(Qt::CursorShape cursorShape, const QPlatformScreen *screen)
{
    Q_ASSERT(cursorShape != Qt::BitmapCursor);

    static const QWindowsStandardCursorMapping standardCursors[] = {
        { Qt::ArrowCursor, IDC_ARROW},
        { Qt::UpArrowCursor, IDC_UPARROW },
        { Qt::CrossCursor, IDC_CROSS },
        { Qt::WaitCursor, IDC_WAIT },
        { Qt::IBeamCursor, IDC_IBEAM },
        { Qt::SizeVerCursor, IDC_SIZENS },
        { Qt::SizeHorCursor, IDC_SIZEWE },
        { Qt::SizeBDiagCursor, IDC_SIZENESW },
        { Qt::SizeFDiagCursor, IDC_SIZENWSE },
        { Qt::SizeAllCursor, IDC_SIZEALL },
        { Qt::ForbiddenCursor, IDC_NO },
        { Qt::WhatsThisCursor, IDC_HELP },
        { Qt::BusyCursor, IDC_APPSTARTING },
        { Qt::PointingHandCursor, IDC_HAND }
    };

    switch (cursorShape) {
    case Qt::BlankCursor: {
        QImage blank = QImage(systemCursorSize(), QImage::Format_Mono);
        blank.fill(0); // ignore color table
        return createBitmapCursor(blank, blank);
    }
    case Qt::SplitVCursor:
    case Qt::SplitHCursor:
    case Qt::OpenHandCursor:
    case Qt::ClosedHandCursor:
    case Qt::DragCopyCursor:
    case Qt::DragMoveCursor:
    case Qt::DragLinkCursor:
        return QWindowsCursor::createPixmapCursor(customCursor(cursorShape, screen));
    default:
        break;
    }

    // Load available standard cursors from resources
    for (const QWindowsStandardCursorMapping &s : standardCursors) {
        if (s.shape == cursorShape) {
            return static_cast<HCURSOR>(LoadImage(nullptr, s.resource, IMAGE_CURSOR,
                                                  0, 0, LR_DEFAULTSIZE | LR_SHARED));
        }
    }

    qWarning("%s: Invalid cursor shape %d", __FUNCTION__, cursorShape);
    return nullptr;
}

/*!
    \brief Return cached standard cursor resources or create new ones.
*/

CursorHandlePtr QWindowsCursor::standardWindowCursor(Qt::CursorShape shape)
{
    StandardCursorCache::Iterator it = m_standardCursorCache.find(shape);
    if (it == m_standardCursorCache.end()) {
        if (const HCURSOR hc = QWindowsCursor::createCursorFromShape(shape, m_screen))
            it = m_standardCursorCache.insert(shape, CursorHandlePtr(new CursorHandle(hc)));
    }
    return it != m_standardCursorCache.end() ? it.value() : CursorHandlePtr(new CursorHandle);
}

HCURSOR QWindowsCursor::m_overriddenCursor = nullptr;
HCURSOR QWindowsCursor::m_overrideCursor = nullptr;

/*!
    \brief Return cached pixmap cursor or create new one.
*/

CursorHandlePtr QWindowsCursor::pixmapWindowCursor(const QCursor &c)
{
    const QWindowsPixmapCursorCacheKey cacheKey(c);
    PixmapCursorCache::iterator it = m_pixmapCursorCache.find(cacheKey);
    if (it == m_pixmapCursorCache.end()) {
        if (m_pixmapCursorCache.size() > 50) {
            // Prevent the cursor cache from growing indefinitely hitting GDI resource
            // limits if new pixmap cursors are created repetitively by purging out
            // all-noncurrent pixmap cursors (QTBUG-43515)
            const HCURSOR currentCursor = GetCursor();
            for (it = m_pixmapCursorCache.begin(); it != m_pixmapCursorCache.end() ; ) {
                if (it.value()->handle() != currentCursor)
                    it = m_pixmapCursorCache.erase(it);
                else
                    ++it;
            }
        }
        const qreal scaleFactor = QHighDpiScaling::factor(m_screen);
        const QPixmap pixmap = c.pixmap();
        const HCURSOR hc = pixmap.isNull()
            ? createBitmapCursor(c, scaleFactor)
            : QWindowsCursor::createPixmapCursor(pixmap, c.hotSpot(), scaleFactor);
        it = m_pixmapCursorCache.insert(cacheKey, CursorHandlePtr(new CursorHandle(hc)));
    }
    return it.value();
}

QWindowsCursor::QWindowsCursor(const QPlatformScreen *screen)
    : m_screen(screen)
{
    static const bool dummy = initResources();
    Q_UNUSED(dummy);
}

inline CursorHandlePtr QWindowsCursor::cursorHandle(const QCursor &cursor)
{
    return cursor.shape() == Qt::BitmapCursor
        ? pixmapWindowCursor(cursor)
        : standardWindowCursor(cursor.shape());
}

/*!
    \brief Set a cursor on a window.

    This is called frequently as the mouse moves over widgets in the window
    (QLineEdits, etc).
*/

void QWindowsCursor::changeCursor(QCursor *cursorIn, QWindow *window)
{
    QWindowsWindow *platformWindow = QWindowsWindow::windowsWindowOf(window);
    if (!platformWindow) // Desktop/Foreign window.
        return;

    if (!cursorIn) {
        platformWindow->setCursor(CursorHandlePtr(new CursorHandle));
        return;
    }
    const CursorHandlePtr wcursor = cursorHandle(*cursorIn);
    if (wcursor->handle()) {
        platformWindow->setCursor(wcursor);
    } else {
        qWarning("%s: Unable to obtain system cursor for %d",
                 __FUNCTION__, cursorIn->shape());
    }
}

// QTBUG-69637: Override cursors can get reset externally when moving across
// window borders. Enforce the cursor again (to be called from enter event).
void QWindowsCursor::enforceOverrideCursor()
{
    if (hasOverrideCursor() && m_overrideCursor != GetCursor())
        SetCursor(m_overrideCursor);
}

void QWindowsCursor::setOverrideCursor(const QCursor &cursor)
{
    const CursorHandlePtr wcursor = cursorHandle(cursor);
    if (const auto overrideCursor = wcursor->handle()) {
        m_overrideCursor = overrideCursor;
        const HCURSOR previousCursor = SetCursor(overrideCursor);
        if (m_overriddenCursor == nullptr)
            m_overriddenCursor = previousCursor;
    } else {
        qWarning("%s: Unable to obtain system cursor for %d",
                 __FUNCTION__, cursor.shape());
    }
}

void QWindowsCursor::clearOverrideCursor()
{
    if (m_overriddenCursor) {
        SetCursor(m_overriddenCursor);
        m_overriddenCursor = m_overrideCursor = nullptr;
    }
    auto &windows = QWindowsContext::instance()->windows();
    for (auto it = windows.cbegin(), end = windows.cend(); it != end; ++it) {
        if (it.value()->screen() == m_screen)
            it.value()->setFlag(QWindowsWindow::RestoreOverrideCursor);
    }
}

QPoint QWindowsCursor::mousePosition()
{
    POINT p;
    GetCursorPos(&p);
    return QPoint(p.x, p.y);
}

QWindowsCursor::State QWindowsCursor::cursorState()
{
    enum { cursorShowing = 0x1, cursorSuppressed = 0x2 }; // Windows 8: CURSOR_SUPPRESSED
    CURSORINFO cursorInfo;
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&cursorInfo)) {
        if (cursorInfo.flags & cursorShowing)
            return State::Showing;
        if (cursorInfo.flags & cursorSuppressed)
            return State::Suppressed;
    }
    return State::Hidden;
}

QPoint QWindowsCursor::pos() const
{
    return mousePosition();
}

void QWindowsCursor::setPos(const QPoint &pos)
{
    SetCursorPos(pos.x() , pos.y());
}

/*
    The standard size is 32x32, even though the cursor is actually just
    16 pixels large. If a large cursor is set in the accessibility settings,
    then the cursor increases with 8 pixels for each step.
*/
QSize QWindowsCursor::size() const
{
    const QPair<DWORD,bool> cursorSizeSetting =
        QWinRegistryKey(HKEY_CURRENT_USER, LR"(Control Panel\Cursors)")
                       .dwordValue(L"CursorBaseSize");
    const int baseSize = screenCursorSize(m_screen).width() / 2;
    if (!cursorSizeSetting.second)
        return QSize(baseSize / 2, baseSize / 2);

    // The registry values are dpi-independent, so we need to scale the result.
    int cursorSizeValue = cursorSizeSetting.first * m_screen->logicalDpi().first
                                                  / m_screen->logicalBaseDpi().first;

    // map from registry value 32-256 to 0-14, and from there to pixels
    cursorSizeValue = (cursorSizeValue - 2 * baseSize) / baseSize;
    const int cursorSize = baseSize + cursorSizeValue * (baseSize / 2);
    return QSize(cursorSize, cursorSize);
}

QPixmap QWindowsCursor::dragDefaultCursor(Qt::DropAction action) const
{
    switch (action) {
    case Qt::CopyAction:
        if (m_copyDragCursor.isNull())
            m_copyDragCursor = QWindowsCursor::customCursor(Qt::DragCopyCursor, m_screen).pixmap;
        return m_copyDragCursor;
    case Qt::TargetMoveAction:
    case Qt::MoveAction:
        if (m_moveDragCursor.isNull())
            m_moveDragCursor = QWindowsCursor::customCursor(Qt::DragMoveCursor, m_screen).pixmap;
        return m_moveDragCursor;
    case Qt::LinkAction:
        if (m_linkDragCursor.isNull())
            m_linkDragCursor = QWindowsCursor::customCursor(Qt::DragLinkCursor, m_screen).pixmap;
        return m_linkDragCursor;
    default:
        break;
    }

    static const char * const ignoreDragCursorXpmC[] = {
    "24 30 3 1",
    ".        c None",
    "a        c #000000",
    "X        c #FFFFFF",
    "aa......................",
    "aXa.....................",
    "aXXa....................",
    "aXXXa...................",
    "aXXXXa..................",
    "aXXXXXa.................",
    "aXXXXXXa................",
    "aXXXXXXXa...............",
    "aXXXXXXXXa..............",
    "aXXXXXXXXXa.............",
    "aXXXXXXaaaa.............",
    "aXXXaXXa................",
    "aXXaaXXa................",
    "aXa..aXXa...............",
    "aa...aXXa...............",
    "a.....aXXa..............",
    "......aXXa.....XXXX.....",
    ".......aXXa..XXaaaaXX...",
    ".......aXXa.XaaaaaaaaX..",
    "........aa.XaaaXXXXaaaX.",
    "...........XaaaaX..XaaX.",
    "..........XaaXaaaX..XaaX",
    "..........XaaXXaaaX.XaaX",
    "..........XaaX.XaaaXXaaX",
    "..........XaaX..XaaaXaaX",
    "...........XaaX..XaaaaX.",
    "...........XaaaXXXXaaaX.",
    "............XaaaaaaaaX..",
    ".............XXaaaaXX...",
    "...............XXXX....."};

    if (m_ignoreDragCursor.isNull()) {
        HCURSOR cursor = LoadCursor(nullptr, IDC_NO);
        ICONINFO iconInfo = {0, 0, 0, nullptr, nullptr};
        GetIconInfo(cursor, &iconInfo);
        BITMAP bmColor = {0, 0, 0, 0, 0, 0, nullptr};

        if (iconInfo.hbmColor
            && GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmColor)
            && bmColor.bmWidth == bmColor.bmWidthBytes / 4) {
            const int colorBitsLength = bmColor.bmHeight * bmColor.bmWidthBytes;
            auto *colorBits = new uchar[colorBitsLength];
            GetBitmapBits(iconInfo.hbmColor, colorBitsLength, colorBits);
            const QImage colorImage(colorBits, bmColor.bmWidth, bmColor.bmHeight,
                                    bmColor.bmWidthBytes, QImage::Format_ARGB32);

            m_ignoreDragCursor = QPixmap::fromImage(colorImage);
            delete [] colorBits;
        } else {
            m_ignoreDragCursor = QPixmap(ignoreDragCursorXpmC);
        }

        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        DestroyCursor(cursor);
    }
    return m_ignoreDragCursor;
}

HCURSOR QWindowsCursor::hCursor(const QCursor &c) const
{
    const Qt::CursorShape shape = c.shape();
    if (shape == Qt::BitmapCursor) {
        const auto pit = m_pixmapCursorCache.constFind(QWindowsPixmapCursorCacheKey(c));
        if (pit != m_pixmapCursorCache.constEnd())
            return pit.value()->handle();
    } else {
        const auto sit = m_standardCursorCache.constFind(shape);
        if (sit != m_standardCursorCache.constEnd())
            return sit.value()->handle();
    }
    return HCURSOR(nullptr);
}

/*!
    \class QWindowsWindowCursor
    \brief Per-Window cursor. Contains a QCursor and manages its associated system
     cursor handle resource.

    \internal
    \sa QWindowsCursor
*/

QT_END_NAMESPACE

#endif // !QT_NO_CURSOR
