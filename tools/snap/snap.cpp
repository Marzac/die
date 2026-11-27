/**
    SNAP
    Headless renderer snapshots for the DIE engine

    Renders each view into one PNG, rows full / walls only / surfaces only.
    Frames are brightened 3x, pixels left undrawn are magenta.
    With a reference folder (snapshots of an earlier build), each view is compared to its
    reference: name_diff.png has columns reference / new / differences in red.

    usage: snap map.map views.txt outdir [refdir]
    views.txt: one view per line: name x y z pan tilt (degrees)
    The renderer config (die.ini) is read and written in outdir.
*/

#include "renderer.h"
#include "map.h"
#include "env.h"
#include "audio.h"

#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cstdio>

class MainWindow;
MainWindow * mainWindow = nullptr;      // map.cpp links against the editor globals

static Map map;
static Env env;

static constexpr int FrameW = 480;
static constexpr int FrameH = 300;

/*****************************************************************************/
static QImage shoot(const Viewpoint & vp, uint32_t flags)
{
// Two frames: the first one rebuilds the glowmap
    Viewpoint v = vp;
    renderer.setFlags((Renderer::FLAGS) (flags | Renderer::FLAG_GLOWMAP_REBUILD));
    env.pass(); map.pass(v.pos); renderer.render(v);
    renderer.setFlags((Renderer::FLAGS) flags);
    env.pass(); map.pass(v.pos); renderer.render(v);

    QImage img = renderer.getImage()->copy().convertToFormat(QImage::Format_RGB32);
    for (int j = 0; j < img.height(); j++) {
        QRgb * line = (QRgb *) img.scanLine(j);
        for (int i = 0; i < img.width(); i++) {
            QRgb c = line[i];
            if ((c & 0xFFFFFF) == 0) {line[i] = 0xFFFF00FF; continue;}
            line[i] = qRgb(std::min(255, qRed(c) * 3), std::min(255, qGreen(c) * 3), std::min(255, qBlue(c) * 3));
        }
    }
    return img;
}

/*****************************************************************************/
int main(int argc, char * argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 4) {
        printf("usage: snap map.map views.txt outdir [refdir]\n");
        return 1;
    }

    QString mapPath = QFileInfo(argv[1]).absoluteFilePath();
    QString viewsPath = QFileInfo(argv[2]).absoluteFilePath();
    QString refPath = argc > 4 ? QFileInfo(argv[4]).absoluteFilePath() : QString();
    QDir().mkpath(argv[3]);
    QDir::setCurrent(argv[3]);

    renderer.init();
    audio.init();
    map.init();
    if (!map.load(mapPath)) {
        printf("cannot load %s\n", qPrintable(mapPath));
        return 1;
    }
    env.init();
    env.sun = map.sun;
    env.fog = map.fog;
    env.glowmap = map.glowmap;
    renderer.setFramesReso(FrameW, FrameH);

    FILE * file = fopen(viewsPath.toLocal8Bit().constData(), "r");
    if (!file) {
        printf("cannot open %s\n", qPrintable(viewsPath));
        return 1;
    }

    char name[64];
    float x, y, z, pan, tilt;
    while (fscanf(file, "%63s %f %f %f %f %f", name, &x, &y, &z, &pan, &tilt) == 6) {
        Viewpoint vp = {};
        vp.pos = QVector3D(x, y, z);
        vp.pan = pan * D2R;
        vp.tilt = tilt * D2R;

        uint32_t base = Renderer::FLAGS_DEFAULT & ~Renderer::FLAG_MULTITHREADING;
        uint32_t hidden[3] = {0, Renderer::FLAG_SURFACES, Renderer::FLAG_WALLS};
        QImage sheet(FrameW, FrameH * 3 + 8, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        QPainter p(&sheet);
        for (int r = 0; r < 3; r++)
            p.drawImage(0, r * (FrameH + 4), shoot(vp, base & ~hidden[r]));
        p.end();
        sheet.save(QString("%1.png").arg(name));

        const Renderer::Levels & l = renderer.levels;
        printf("%s: floor %.2f, ceiling %.2f", name, l.floor.h0, l.ceiling.h0);
        if (refPath.isEmpty()) {printf("\n"); continue;}

    // Compare with the reference sheet
        QImage ref = QImage(QString("%1/%2.png").arg(refPath, name)).convertToFormat(QImage::Format_RGB32);
        if (ref.size() != sheet.size()) {printf(", no matching reference\n"); continue;}
        QImage diff(sheet.size(), QImage::Format_RGB32);
        int count = 0;
        for (int j = 0; j < sheet.height(); j++)
            for (int i = 0; i < sheet.width(); i++) {
                bool d = ref.pixel(i, j) != sheet.pixel(i, j);
                if (j < FrameH) count += d;
                diff.setPixel(i, j, d ? 0xFFFF0000 : 0xFF000000 | (qGray(ref.pixel(i, j)) / 3) * 0x010101);
            }
        printf(", %d px differ\n", count);

        QImage cmp(FrameW * 3 + 8, sheet.height(), QImage::Format_RGB32);
        cmp.fill(Qt::white);
        QPainter c(&cmp);
        c.drawImage(0, 0, ref);
        c.drawImage(FrameW + 4, 0, sheet);
        c.drawImage(2 * FrameW + 8, 0, diff);
        c.end();
        cmp.save(QString("%1_diff.png").arg(name));
    }
    fclose(file);
    return 0;
}
