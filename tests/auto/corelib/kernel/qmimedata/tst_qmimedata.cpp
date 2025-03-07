// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QTest>

#include <QMimeData>

class tst_QMimeData : public QObject
{
    Q_OBJECT
private slots:
    void clear() const;
    void colorData() const;
    void data() const;
    void formats() const;
    void hasColor() const;
    void hasFormat() const;
    void hasHtml() const;
    void hasImage() const;
    // hasText() covered by setText()
    // hasUrls() covered by setUrls()
    // html() covered by setHtml()
    void imageData() const;
    void removeFormat() const;
    // setColorData() covered by hasColor()
    // setData() covered in a few different tests
    void setHtml() const;
    // setImageData() covered in a few tests
    void setText() const;
    void setUrls() const;
    // text() covered in setText()
    // urls() covered by setUrls()
};

void tst_QMimeData::clear() const
{
    QMimeData mimeData;

    // set, clear, verify empty
    mimeData.setData("text/plain", "pirates");
    QVERIFY(mimeData.hasText());
    mimeData.clear();
    QVERIFY(!mimeData.hasText());

    // repopulate, verify not empty
    mimeData.setData("text/plain", "pirates");
    QVERIFY(mimeData.hasText());
}

void tst_QMimeData::colorData() const
{
    QMimeData mimeData;
    QColor red = Qt::red;
    QColor blue = Qt::blue;

    // set, verify
    mimeData.setColorData(red);
    QVERIFY(mimeData.hasColor());
    QCOMPARE(qvariant_cast<QColor>(mimeData.colorData()), red);

    // change, verify
    mimeData.setColorData(QColor(Qt::blue));
    QVERIFY(mimeData.hasColor());
    QCOMPARE(qvariant_cast<QColor>(mimeData.colorData()), blue);
}

void tst_QMimeData::data() const
{
    QMimeData mimeData;

    // set text, verify
    mimeData.setData("text/plain", "pirates");
    QCOMPARE(mimeData.data("text/plain"), QByteArray("pirates"));
    QCOMPARE(mimeData.data("text/html").size(), 0);
    QCOMPARE(mimeData.data("text/markdown").size(), 0);

    // html time
    mimeData.setData("text/html", "ninjas");
    QCOMPARE(mimeData.data("text/html"), QByteArray("ninjas"));
    QCOMPARE(mimeData.data("text/plain"), QByteArray("pirates")); // make sure text not damaged
    QCOMPARE(mimeData.data("text/html"), mimeData.html().toLatin1());

    // markdown time
    mimeData.setData("text/markdown", "vikings");
    QCOMPARE(mimeData.data("text/markdown"), QByteArray("vikings"));
    QCOMPARE(mimeData.data("text/html"), QByteArray("ninjas"));
    QCOMPARE(mimeData.data("text/plain"), QByteArray("pirates"));

    // URI list
    QByteArray list = "https://example.com/\r\nhttps://example.net/\r\nhttps://example.org/\r\n";
    mimeData.setData("text/uri-list", list);
    QCOMPARE(mimeData.data("text/uri-list"), list);

    mimeData.setData("text/uri-list", list.chopped(2)); // without the ending CRLF
    QCOMPARE(mimeData.data("text/uri-list"), list);
}

void tst_QMimeData::formats() const
{
    QMimeData mimeData;

    // set text, verify
    mimeData.setData("text/plain", "pirates");
    QCOMPARE(mimeData.formats(), QStringList() << "text/plain");

    // set html, verify
    mimeData.setData("text/html", "ninjas");
    QCOMPARE(mimeData.formats(), QStringList() << "text/plain" << "text/html");

    // set markdown, verify
    mimeData.setData("text/markdown", "vikings");
    QCOMPARE(mimeData.formats(), QStringList() << "text/plain" << "text/html" << "text/markdown");

    // clear, verify
    mimeData.clear();
    QCOMPARE(mimeData.formats(), QStringList());

    // set an odd format, verify
    mimeData.setData("foo/bar", "somevalue");
    QCOMPARE(mimeData.formats(), QStringList() << "foo/bar");
}

void tst_QMimeData::hasColor() const
{
    QMimeData mimeData;

    // initial state
    QVERIFY(!mimeData.hasColor());

    // set, verify
    mimeData.setColorData(QColor(Qt::red));
    QVERIFY(mimeData.hasColor());

    // clear, verify
    mimeData.clear();
    QVERIFY(!mimeData.hasColor());

    // set something else, verify
    mimeData.setData("text/plain", "pirates");
    QVERIFY(!mimeData.hasColor());
}

void tst_QMimeData::hasFormat() const
{
    QMimeData mimeData;

    // initial state
    QVERIFY(!mimeData.hasFormat("text/plain"));

    // add, verify
    mimeData.setData("text/plain", "pirates");
    QVERIFY(mimeData.hasFormat("text/plain"));
    QVERIFY(!mimeData.hasFormat("text/html"));

    // clear, verify
    mimeData.clear();
    QVERIFY(!mimeData.hasFormat("text/plain"));
    QVERIFY(!mimeData.hasFormat("text/html"));
}

void tst_QMimeData::hasHtml() const
{
    QMimeData mimeData;

    // initial state
    QVERIFY(!mimeData.hasHtml());

    // add plain, verify false
    mimeData.setData("text/plain", "pirates");
    QVERIFY(!mimeData.hasHtml());

    // add html, verify
    mimeData.setData("text/html", "ninjas");
    QVERIFY(mimeData.hasHtml());

    // clear, verify
    mimeData.clear();
    QVERIFY(!mimeData.hasHtml());

    // readd, verify
    mimeData.setData("text/html", "ninjas");
    QVERIFY(mimeData.hasHtml());
}

void tst_QMimeData::hasImage() const
{
    QMimeData mimeData;

    // initial state
    QVERIFY(!mimeData.hasImage());

    // add text, verify false
    mimeData.setData("text/plain", "pirates");
    QVERIFY(!mimeData.hasImage());

    // add image
    mimeData.setImageData(QImage());
    QVERIFY(mimeData.hasImage());

    // clear, verify
    mimeData.clear();
    QVERIFY(!mimeData.hasImage());
}

void tst_QMimeData::imageData() const
{
    QMimeData mimeData;

    // initial state
    QCOMPARE(mimeData.imageData(), QVariant());

    // set, test
    mimeData.setImageData(QImage());
    QVERIFY(mimeData.hasImage());
    QCOMPARE(mimeData.imageData(), QVariant(QImage()));

    // clear, verify
    mimeData.clear();
    QCOMPARE(mimeData.imageData(), QVariant());
}

void tst_QMimeData::removeFormat() const
{
    QMimeData mimeData;

    // add, verify
    mimeData.setData("text/plain", "pirates");
    QVERIFY(mimeData.hasFormat("text/plain"));

    // add another, verify
    mimeData.setData("text/html", "ninjas");
    QVERIFY(mimeData.hasFormat("text/html"));

    // remove, verify
    mimeData.removeFormat("text/plain");
    QVERIFY(!mimeData.hasFormat("text/plain"));
    QVERIFY(mimeData.hasFormat("text/html"));

    // remove, verify
    mimeData.removeFormat("text/html");
    QVERIFY(!mimeData.hasFormat("text/plain"));
    QVERIFY(!mimeData.hasFormat("text/html"));
}

void tst_QMimeData::setHtml() const
{
    QMimeData mimeData;

    // initial state
    QVERIFY(!mimeData.hasHtml());

    // add html, verify
    mimeData.setHtml("ninjas");
    QVERIFY(mimeData.hasHtml());
    QCOMPARE(mimeData.html(), QLatin1String("ninjas"));

    // reset html
    mimeData.setHtml("pirates");
    QVERIFY(mimeData.hasHtml());
    QCOMPARE(mimeData.html(), QLatin1String("pirates"));
}

void tst_QMimeData::setText() const
{
    QMimeData mimeData;

    // verify initial state
    QCOMPARE(mimeData.text(), QLatin1String(""));
    QVERIFY(!mimeData.hasText());

    // set, verify
    mimeData.setText("pirates");
    QVERIFY(mimeData.hasText());
    QCOMPARE(mimeData.text(), QLatin1String("pirates"));
    QCOMPARE(mimeData.text().toLatin1(), mimeData.data("text/plain"));

    // reset, verify
    mimeData.setText("ninjas");
    QVERIFY(mimeData.hasText());
    QCOMPARE(mimeData.text(), QLatin1String("ninjas"));
    QCOMPARE(mimeData.text().toLatin1(), mimeData.data("text/plain"));

    // clear, verify
    mimeData.clear();
    QCOMPARE(mimeData.text(), QLatin1String(""));
    QVERIFY(!mimeData.hasText());
}

// Publish retrieveData for verifying content validity
class TstMetaData : public QMimeData
{
public:
    using QMimeData::retrieveData;
};

void tst_QMimeData::setUrls() const
{
    TstMetaData mimeData;
    QList<QUrl> shortUrlList;
    QList<QUrl> longUrlList;

    // set up
    shortUrlList += QUrl("http://qt-project.org");
    longUrlList = shortUrlList;
    longUrlList += QUrl("http://www.google.com");

    // verify initial state
    QCOMPARE(mimeData.hasUrls(), false);

    // set a few, verify
    mimeData.setUrls(shortUrlList);
    QCOMPARE(mimeData.urls(), shortUrlList);
    QCOMPARE(mimeData.text(), QString("http://qt-project.org"));

    // change them, verify
    mimeData.setUrls(longUrlList);
    QCOMPARE(mimeData.urls(), longUrlList);
    QCOMPARE(mimeData.text(), QString("http://qt-project.org\nhttp://www.google.com\n"));

    // test and verify that setData doesn't corrupt url content
    const auto allFormats = mimeData.formats();
    for (const QString &format : allFormats) {
         QVariant before = mimeData.retrieveData(format, QMetaType(QMetaType::QByteArray));
         mimeData.setData(format, mimeData.data(format));
         QVariant after = mimeData.retrieveData(format, QMetaType(QMetaType::QByteArray));
         QCOMPARE(after, before);
     }

    // clear, verify
    mimeData.clear();
    QCOMPARE(mimeData.hasUrls(), false);
    QCOMPARE(mimeData.hasText(), false);
}

QTEST_APPLESS_MAIN(tst_QMimeData)
#include "tst_qmimedata.moc"
