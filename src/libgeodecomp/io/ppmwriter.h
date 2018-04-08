#ifndef LIBGEODECOMP_IO_PPMWRITER_H
#define LIBGEODECOMP_IO_PPMWRITER_H

#include <libgeodecomp/io/imagepainter.h>
#include <libgeodecomp/io/ioexception.h>
#include <libgeodecomp/io/plotter.h>
#include <libgeodecomp/io/simplecellplotter.h>
#include <libgeodecomp/io/writer.h>
#include <libgeodecomp/misc/clonable.h>
#include <libgeodecomp/misc/quickpalette.h>
#include <libgeodecomp/storage/image.h>

// Kill warning 4514 in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

#include <cerrno>
#include <fstream>
#include <iomanip>
#include <string>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

namespace LibGeoDecomp {

/**
 * This writer will periodically write images in PPM format. The
 * CELL_PLOTTER is responsible for rendering individual cells into
 * tiles. The default will render uniformly colored tiles.
 */
template<typename CELL_TYPE, typename CELL_PLOTTER = SimpleCellPlotter<CELL_TYPE> >
class PPMWriter : public Clonable<Writer<CELL_TYPE>, PPMWriter<CELL_TYPE, CELL_PLOTTER> >
{
public:
    friend class PPMWriterTest;

    typedef typename Writer<CELL_TYPE>::GridType GridType;
    using Writer<CELL_TYPE>::period;
    using Writer<CELL_TYPE>::prefix;

    /**
     * This PPMWriter will render a given member (e.g. &Cell::fooBar).
     * Colouring is handled by a predefined palette. The color range
     * is mapped to the value range defined by [minValue, maxValue].
     */
    template<typename MEMBER>
    PPMWriter(
        MEMBER CELL_TYPE:: *member,
        MEMBER minValue,
        MEMBER maxValue,
        const std::string& prefix,
        const unsigned period = 1,
        const Coord<2>& cellDimensions = Coord<2>(8, 8)) :
        Clonable<Writer<CELL_TYPE>, PPMWriter<CELL_TYPE, CELL_PLOTTER> >(prefix, period),
        plotter(cellDimensions, CELL_PLOTTER(member, QuickPalette<MEMBER>(minValue, maxValue)))
    {}

    /**
     * Creates a PPMWriter which will render the values of the given
     * member variable. Color mapping is done with the help of the
     * custom palette object.
     */
    template<typename MEMBER, typename PALETTE>
    PPMWriter(
        MEMBER CELL_TYPE:: *member,
        const PALETTE& palette,
        const std::string& prefix,
        const unsigned period = 1,
        const Coord<2>& cellDimensions = Coord<2>(8, 8)) :
        Clonable<Writer<CELL_TYPE>, PPMWriter<CELL_TYPE, CELL_PLOTTER> >(prefix, period),
       plotter(cellDimensions, CELL_PLOTTER(member, palette))
    {}

    virtual void stepFinished(const GridType& grid, unsigned step, WriterEvent event)
    {
        if ((event == WRITER_STEP_FINISHED) && (step % period != 0)) {
            return;
        }

        Coord<2> imageDim = plotter.calcImageDim(grid.boundingBox().dimensions);
        Image image(imageDim);
        ImagePainter painter(&image);
        plotter.plotGrid(grid, painter);
        writePPM(image, step);
    }

 private:
    Plotter<CELL_TYPE, CELL_PLOTTER> plotter;

    void writePPM(const Image& img, unsigned step)
    {
        std::ostringstream filename;
        filename << prefix << "." << std::setfill('0') << std::setw(4)
                 << step << ".ppm";
        std::ofstream outfile(filename.str().c_str());
        if (!outfile) {
            throw FileOpenException(filename.str());
        }

        // header first:
        outfile << "P6 " << img.getDimensions().x()
                << " "   << img.getDimensions().y() << " 255\n";

        // body second:
        for (int y = 0; y < img.getDimensions().y(); ++y) {
            for (int x = 0; x < img.getDimensions().x(); ++x) {
                const Color& rgb = img[Coord<2>(x, y)];
                outfile << (char)rgb.red()
                        << (char)rgb.green()
                        << (char)rgb.blue();
            }
        }

        if (!outfile.good()) {
            throw FileWriteException(filename.str());
        }
        outfile.close();
    }
};

}

#endif
