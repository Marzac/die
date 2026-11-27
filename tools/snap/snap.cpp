/**
    SNAP
    Headless renderer snapshots for the DIE engine

    Renders each view with renderVertical and renderVertical5 into one PNG
    (renderVertical5 and renderVertical6 with rv6):
    columns old / new / differences in red, rows full / walls only / surfaces only.
    Frames are brightened 3x, pixels left undrawn are magenta.

    usage: snap map.map views.txt outdir [rv6]
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
        printf("usage: snap map.map views.txt outdir [rv6]\n");
        return 1;
    }
    bool rv6 = argc > 4 && QString(argv[4]) == "rv6";
    uint32_t oldAlgo = rv6 ? Renderer::FLAG_APERTURES : 0;
    uint32_t newAlgo = rv6 ? Renderer::FLAG_APERTURES6 : Renderer::FLAG_APERTURES;

    QString mapPath = QFileInfo(argv[1]).absoluteFilePath();
    QString viewsPath = QFileInfo(argv[2]).absoluteFilePath();
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
        QImage sheet(FrameW * 3 + 8, FrameH * 3 + 8, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        QPainter p(&sheet);
        int count = 0;

        for (int r = 0; r < 3; r++) {
            QImage o = shoot(vp, (base & ~hidden[r]) | oldAlgo);
            QImage n = shoot(vp, (base & ~hidden[r]) | newAlgo);
            QImage diff(FrameW, FrameH, QImage::Format_RGB32);
            for (int j = 0; j < FrameH; j++)
                for (int i = 0; i < FrameW; i++) {
                    bool d = o.pixel(i, j) != n.pixel(i, j);
                    if (r == 0) count += d;
                    diff.setPixel(i, j, d ? 0xFFFF0000 : 0xFF000000 | (qGray(o.pixel(i, j)) / 3) * 0x010101);
                }
            int top = r * (FrameH + 4);
            p.drawImage(0, top, o);
            p.drawImage(FrameW + 4, top, n);
            p.drawImage(2 * FrameW + 8, top, diff);
        }
        p.end();

        sheet.save(QString("%1.png").arg(name));
        const Renderer::Levels & l = renderer.levels;
        printf("%s: %d px differ, floor %.2f, ceiling %.2f\n", name, count, l.floor.h0, l.ceiling.h0);
    }
    fclose(file);
    return 0;
}
