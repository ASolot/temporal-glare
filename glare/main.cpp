#include <QApplication>

#include <iostream>
#include <QApplication>
#include <QSurfaceFormat>
#include <QMatrix4x4>
#include <QVector3D>
#include <QCommandLineParser>

#include "TGViewerWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Temporal Glare Viewer");
    parser.addHelpOption();

    parser.process(app);


    QSurfaceFormat fmt;
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    // QString lf_dir = "lf-images";
    // const QStringList args = parser.positionalArguments();
    // if( args.length()>0 )
    //     lf_dir=args[0];

    TGViewerWindow window;
    window.show();
    return app.exec();

}
