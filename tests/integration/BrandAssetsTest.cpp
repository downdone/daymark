#include "platform/common/BrandIcons.h"
#include <QtTest>
#include <QImage>
#include <QFile>
#include <QImageReader>
#include <QXmlStreamReader>

class BrandAssetsTest final:public QObject {
    Q_OBJECT
private slots:
    void applicationSizesAndNeutralMicroArtwork() {
        const auto icon=daymark::BrandIcons::application();QVERIFY(!icon.isNull());
        for(const int n:{16,20,24,32,48,64,128,256,512,1024}) {
            QImage image(QStringLiteral(":/daymark/icons/daymark-%1.png").arg(n));
            QCOMPARE(image.size(),QSize(n,n));QVERIFY(image.hasAlphaChannel());
            QVERIFY(image.pixelColor(0,0).alpha()<=4);QCOMPARE(image.pixelColor(n/2,n/2).alpha(),255);
            QVERIFY(!icon.pixmap(n,n).isNull());
            if(n<48)for(int y=0;y<n;++y)for(int x=0;x<n;++x){const auto c=image.pixelColor(x,y);if(c.alpha()>100)QVERIFY(qMax(c.red(),qMax(c.green(),c.blue()))-qMin(c.red(),qMin(c.green(),c.blue()))<=4);}
        }
    }
    void fullSizeHasApprovedFourColors() {
        QImage image(QStringLiteral(":/daymark/icons/daymark-256.png"));
        for(const QColor expected:{QColor("#287EF0"),QColor("#28A680"),QColor("#E4A13C"),QColor("#A783DE")}) {
            int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y)==expected)++count;
            QVERIFY2(count>4,qPrintable(expected.name()));
        }
    }
    void trayIsMonochromeAndHasBothSurfaceVariants() {
        for(const bool light:{false,true}) {
            const auto icon=daymark::BrandIcons::tray(light);QVERIFY(!icon.isNull());
            for(const int n:{16,20,24,32}) {
                const QImage image(QStringLiteral(":/daymark/icons/tray-%1-%2.png").arg(light?"black":"white").arg(n));
                QCOMPARE(image.size(),QSize(n,n));int painted=0;
                for(int y=0;y<n;++y)for(int x=0;x<n;++x){const auto c=image.pixelColor(x,y);if(c.alpha()>100){++painted;if(light)QVERIFY(c.lightness()<80);else QVERIFY(c.lightness()>240);}}
                QVERIFY(painted>6);
            }
        }
    }
    void nativeContainersAndIndependentVectors() {
        const auto icoPath=QFINDTESTDATA("../../assets/icons/daymark.ico");QVERIFY(!icoPath.isEmpty());
        QFile ico(icoPath);QVERIFY(ico.open(QIODevice::ReadOnly));const auto header=ico.read(6);QCOMPARE(header.left(4),QByteArray("\0\0\1\0",4));QVERIFY(quint8(header[4])>=8);
        QFile icns(QFINDTESTDATA("../../assets/icons/daymark.icns"));QVERIFY(icns.open(QIODevice::ReadOnly));const auto data=icns.readAll();QCOMPARE(data.left(4),QByteArray("icns"));
        const auto read32=[](const QByteArray& d,int i){return (quint32(quint8(d[i]))<<24)|(quint32(quint8(d[i+1]))<<16)|(quint32(quint8(d[i+2]))<<8)|quint32(quint8(d[i+3]));};
        QCOMPARE(read32(data,4),quint32(data.size()));int representations=0;
        for(int at=8;at<data.size();) {const auto size=read32(data,at+4);QVERIFY(size>8 && at+int(size)<=data.size());QVERIFY(!QImage::fromData(data.mid(at+8,size-8),"PNG").isNull());at+=size;++representations;}
        QCOMPARE(representations,11);
        QFile svg(QFINDTESTDATA("../../assets/brand/current/master/symbol-primary.svg"));QVERIFY(svg.open(QIODevice::ReadOnly));QXmlStreamReader reader(&svg);int paths=0;
        while(!reader.atEnd()){reader.readNext();if(!reader.isStartElement())continue;const auto name=reader.name();QVERIFY(name=="svg" || name=="title" || name=="path" || name=="g");if(name=="path")++paths;for(const auto& a:reader.attributes())QVERIFY(!a.name().contains(u"href"));}
        QVERIFY(!reader.hasError());QCOMPARE(paths,6);
    }
};
QTEST_MAIN(BrandAssetsTest)
#include "BrandAssetsTest.moc"
