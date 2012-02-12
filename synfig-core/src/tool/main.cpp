/* === S Y N F I G ========================================================= */
/*!	\file tool/main.cpp
**	\brief SYNFIG Tool
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2009-2012 Diego Barrios Romero
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <iostream>
#include <ETL/stringf>
#include <list>
#include <ETL/clock>
#include <algorithm>
#include <cstring>
#include <errno.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>

#include <synfig/loadcanvas.h>
#include <synfig/savecanvas.h>
#include <synfig/target_scanline.h>
#include <synfig/module.h>
#include <synfig/importer.h>
#include <synfig/layer.h>
#include <synfig/canvas.h>
#include <synfig/target.h>
#include <synfig/targetparam.h>
#include <synfig/time.h>
#include <synfig/string.h>
#include <synfig/paramdesc.h>
#include <synfig/main.h>
#include <synfig/guid.h>
#include <autorevision.h>
#include "definitions.h"
#include "progress.h"
#include "renderprogress.h"
#include "job.h"

#include "named_type.h"
#endif

using namespace std;
using namespace etl;
using namespace synfig;
using namespace boost;
namespace po=boost::program_options;

/* === G L O B A L S ================================================ */

const char *progname;
int verbosity=0;
bool be_quiet=false;
bool print_benchmarks=false;

//! Allowed video codecs
/*! \warning This variable is linked to allowed_video_codecs_description,
 *  if you change this you must change the other acordingly.
 *  \warning These codecs are linked to the filename extensions for
 *  mod_ffmpeg. If you change this you must change the others acordingly.
 */
const char* allowed_video_codecs[] =
{
	"flv", "h263p", "huffyuv", "libtheora", "libx264",
	"mjpeg", "mpeg1video", "mpeg2video", "mpeg4", "msmpeg4",
	"msmpeg4v1", "msmpeg4v2", "wmv1", "wmv2", NULL
};

//! Allowed video codecs description.
/*! \warning This variable is linked to allowed_video_codecs,
 *  if you change this you must change the other acordingly.
 */
const char* allowed_video_codecs_description[] =
{
	"Flash Video (FLV) / Sorenson Spark / Sorenson H.263.",
	"H.263+ / H.263-1998 / H.263 version 2.",
	"Huffyuv / HuffYUV.",
	"libtheora Theora.",
	"libx264 H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10.",
	"MJPEG (Motion JPEG).",
	"raw MPEG-1 video.",
	"raw MPEG-2 video.",
	"MPEG-4 part 2. (XviD/DivX)",
	"MPEG-4 part 2 Microsoft variant version 3.",
	"MPEG-4 part 2 Microsoft variant version 1.",
	"MPEG-4 part 2 Microsoft variant version 2.",
	"Windows Media Video 7.",
	"Windows Media Video 8.",
	NULL
};

/* === M E T H O D S ================================================ */

#ifdef _DEBUG

void guid_test()
{
	cout << "GUID Test" << endl;
	for(int i = 20; i; i--)
		cout << synfig::GUID().get_string() << ' '
			 << synfig::GUID().get_string() << endl;
}

void signal_test_func()
{
	cout << "**SIGNAL CALLED**" << endl;
}

void signal_test()
{
	sigc::signal<void> sig;
	sigc::connection conn;
	cout << "Signal Test" << endl;
	conn = sig.connect(sigc::ptr_fun(signal_test_func));
	cout << "Next line should exclaim signal called." << endl;
	sig();
	conn.disconnect();
	cout << "Next line should NOT exclaim signal called." << endl;
	sig();
	cout << "done."<<endl;
}

#endif

void print_usage ()
{
	cout << "Synfig " << VERSION << endl
		 << "Usage: " << progname
		 << " [options] ([sif file] [specific options])" << endl;
}

void print_target_video_codecs_help ()
{
	for (int i = 0; allowed_video_codecs[i] != NULL &&
					allowed_video_codecs_description[i] != NULL; i++)
		cout << " " << allowed_video_codecs[i] << ":   \t"
			 << allowed_video_codecs_description[i]
			 << endl;
}

/// Print canvases' children IDs in cascade
void print_child_canvases(string prefix, Canvas::Handle canvas)
{
	Canvas::Children children(canvas->children());
	for (Canvas::Children::iterator child_canvas = children.begin();
		 child_canvas != children.end(); child_canvas++)
	{
		cout << prefix << ":" << (*child_canvas)->get_id() << endl;
		print_child_canvases(prefix + ":" + (*child_canvas)->get_id(),
							*child_canvas);
	}
}

void print_canvas_info(Job job)
{
	Canvas::Handle canvas(job.canvas);
	const RendDesc &rend_desc(canvas->rend_desc());

	if (job.canvas_info_all || job.canvas_info_time_start)
	{
		cout << endl << "# " << _("Start Time") << endl;
		cout << "time_start" << "="
			 << rend_desc.get_time_start().get_string().c_str() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_time_end)
	{
		cout << endl << "# " << _("End Time") << endl;
		cout << "time_end" << "="
			 << rend_desc.get_time_end().get_string().c_str() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_frame_rate)
	{
		cout << endl << "# " << _("Frame Rate") << endl;
		cout << "frame_rate" << "="
			 << rend_desc.get_frame_rate() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_frame_start)
	{
		cout << endl << "# " << _("Start Frame") << endl;
		cout << "frame_start" << "="
			 << rend_desc.get_frame_start() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_frame_end)
	{
		cout << endl << "# " << _("End Frame") << endl;
		cout << "frame_end"	<< "="
			 << rend_desc.get_frame_end() << endl;
	}

	if (job.canvas_info_all)
		cout << endl;

	if (job.canvas_info_all || job.canvas_info_w)
	{
		cout << endl << "# " << _("Width") << endl;
		cout << "w"	<< "=" << rend_desc.get_w() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_h)
	{
		cout << endl << "# " << _("Height") << endl;
		cout << "h"	<< "=" << rend_desc.get_h() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_image_aspect)
	{
		cout << endl << "# " << _("Image Aspect Ratio") << endl;
		cout << "image_aspect" << "="
			 << rend_desc.get_image_aspect() << endl;
	}

	if (job.canvas_info_all)
		cout << endl;

	if (job.canvas_info_all || job.canvas_info_pw)
	{
		cout << endl << "# " << _("Pixel Width") << endl;
		cout << "pw" << "="
			 << rend_desc.get_pw() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_ph)
	{
		cout << endl << "# " << _("Pixel Height") << endl;
		cout << "ph" << "="
			 << rend_desc.get_ph() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_pixel_aspect)
	{
		cout << endl << "# " << _("Pixel Aspect Ratio") << endl;
		cout << "pixel_aspect" << "="
			 << rend_desc.get_pixel_aspect() << endl;
	}

	if (job.canvas_info_all)
		cout << endl;

	if (job.canvas_info_all || job.canvas_info_tl)
	{
		cout << endl << "# " << _("Top Left") << endl;
		cout << "tl" << "=" << rend_desc.get_tl()[0]
			 << " " << rend_desc.get_tl()[1] << endl;
	}

	if (job.canvas_info_all || job.canvas_info_br)
	{
		cout << endl << "# " << _("Bottom Right") << endl;
		cout << "br" << "=" << rend_desc.get_br()[0]
			 << " " << rend_desc.get_br()[1] << endl;
	}

	if (job.canvas_info_all || job.canvas_info_physical_w)
	{
		cout << endl << "# " << _("Physical Width") << endl;
		cout << "physical_w" << "="
			 << rend_desc.get_physical_w() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_physical_h)
	{
		cout << endl << "# " << _("Physical Height") << endl;
		cout << "physical_h" << "="
			 << rend_desc.get_physical_h() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_x_res)
	{
		cout << endl << "# " << _("X Resolution") << endl;
		cout << "x_res"	<< "=" << rend_desc.get_x_res() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_y_res)
	{
		cout << endl << "# " << _("Y Resolution") << endl;
		cout << "y_res"	<< "=" << rend_desc.get_y_res() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_span)
	{
		cout << endl << "# " << _("Diagonal Image Span") << endl;
		cout << "span" << "=" << rend_desc.get_span() << endl;
	}

	if (job.canvas_info_all)
		cout << endl;

	if (job.canvas_info_all || job.canvas_info_interlaced)
	{
		cout << endl << "# " << _("Interlaced") << endl;
		cout << "interlaced" << "="
			 << rend_desc.get_interlaced() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_antialias)
	{
		cout << endl << "# " << _("Antialias") << endl;
		cout << "antialias"	<< "="
			 << rend_desc.get_antialias() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_clamp)
	{
		cout << endl << "# " << _("Clamp") << endl;
		cout << "clamp"
			 << "=" << rend_desc.get_clamp() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_flags)
	{
		cout << endl << "# " << _("Flags") << endl;
		cout << "flags"
			 << "=" << rend_desc.get_flags() << endl;
	}

	if (job.canvas_info_all || job.canvas_info_focus)
	{
		cout << endl << "# " << _("Focus") << endl;
		cout << "focus"	<< "=" << rend_desc.get_focus()[0]
			 << " " << rend_desc.get_focus()[1] << endl;
	}

	if (job.canvas_info_all || job.canvas_info_bg_color)
	{
		cout << endl << "# " << _("Background Color") << endl;
		cout << "bg_color" << "="
			 << rend_desc.get_bg_color().get_string().c_str() << endl;
	}

	if (job.canvas_info_all)
		cout << endl;

	if (job.canvas_info_all || job.canvas_info_metadata)
	{
		std::list<String> keys(canvas->get_meta_data_keys());
		cout << endl << "# " << _("Metadata") << endl;

		for (std::list<String>::iterator key = keys.begin();
			 key != keys.end(); key++)
			cout << *key << "=" << canvas->get_meta_data(*key) << endl;
	}
}

void extract_canvas_info(Job& job, string values)
{
	job.canvas_info = true;
	string value;

	std::string::size_type pos;
	while (!values.empty())
	{
		pos = values.find_first_of(',');
		if (pos == std::string::npos)
		{
			value = values;
			values = "";
		}
		else
		{
			value = values.substr(0, pos);
			values = values.substr(pos+1);
		}
		if (value == "all")
		{
			job.canvas_info_all = true;
			return;
		}

		if (value == "time_start")			job.canvas_info_time_start		= true;
		else if (value == "time_end")		job.canvas_info_time_end		= true;
		else if (value == "frame_rate")		job.canvas_info_frame_rate		= true;
		else if (value == "frame_start")	job.canvas_info_frame_start		= true;
		else if (value == "frame_end")		job.canvas_info_frame_end		= true;
		else if (value == "w")				job.canvas_info_w				= true;
		else if (value == "h")				job.canvas_info_h				= true;
		else if (value == "image_aspect")	job.canvas_info_image_aspect	= true;
		else if (value == "pw")				job.canvas_info_pw				= true;
		else if (value == "ph")				job.canvas_info_ph				= true;
		else if (value == "pixel_aspect")	job.canvas_info_pixel_aspect	= true;
		else if (value == "tl")				job.canvas_info_tl				= true;
		else if (value == "br")				job.canvas_info_br				= true;
		else if (value == "physical_w")		job.canvas_info_physical_w		= true;
		else if (value == "physical_h")		job.canvas_info_physical_h		= true;
		else if (value == "x_res")			job.canvas_info_x_res			= true;
		else if (value == "y_res")			job.canvas_info_y_res			= true;
		else if (value == "span")			job.canvas_info_span			= true;
		else if (value == "interlaced")		job.canvas_info_interlaced		= true;
		else if (value == "antialias")		job.canvas_info_antialias		= true;
		else if (value == "clamp")			job.canvas_info_clamp			= true;
		else if (value == "flags")			job.canvas_info_flags			= true;
		else if (value == "focus")			job.canvas_info_focus			= true;
		else if (value == "bg_color")		job.canvas_info_bg_color		= true;
		else if (value == "metadata")		job.canvas_info_metadata		= true;
		else
		{
			cerr<<_("Unrecognised canvas variable: ") << "'" << value << "'" << endl;
			cerr<<_("Recognized variables are:") << endl <<
				"  all, time_start, time_end, frame_rate, frame_start, frame_end, w, h," << endl <<
				"  image_aspect, pw, ph, pixel_aspect, tl, br, physical_w, physical_h," << endl <<
				"  x_res, y_res, span, interlaced, antialias, clamp, flags," << endl <<
				"  focus, bg_color, metadata" << endl;
		}

		if (pos == std::string::npos)
			break;
	};
}

int main(int ac, char* av[])
{
	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	bindtextdomain("synfig", LOCALEDIR);
	bind_textdomain_codeset("synfig", "UTF-8");
	textdomain("synfig");
#endif

	progname=av[0];

	if(!SYNFIG_CHECK_VERSION())
	{
		cerr << _("FATAL: Synfig Version Mismatch") << endl;
		return SYNFIGTOOL_BADVERSION;
	}

	if(ac==1)
	{
		print_usage();
		return SYNFIGTOOL_BLANK;
	}

    try {
		named_type<string>* target = new named_type<string>("module");
		named_type<int>* width = new named_type<int>("NUM");
		named_type<int>* height = new named_type<int>("NUM");
		named_type<int>* span = new named_type<int>("NUM");
		named_type<int>* antialias = new named_type<int>("1..30");
		named_type<int>* quality = new named_type<int>("0..10");
		named_type<float>* gamma = new named_type<float>("NUM (=2.2)");
		named_type<int>* threads = new named_type<int>("NUM");
		named_type<string>* canvas = new named_type<string>("canvas-id");
		named_type<string>* output_file = new named_type<string>("filename");
		named_type<string>* input_file = new named_type<string>("filename");
		named_type<int>* fps = new named_type<int>("NUM");
		named_type<int>* time = new named_type<int>("seconds");
		named_type<int>* begin_time = new named_type<int>("seconds");
		named_type<int>* start_time = new named_type<int>("seconds");
		named_type<int>* end_time = new named_type<int>("seconds");
		named_type<int>* dpi = new named_type<int>("NUM");
		named_type<int>* dpi_x = new named_type<int>("NUM");
		named_type<int>* dpi_y = new named_type<int>("NUM");
		named_type<string>* append_filename = new named_type<string>("filename");
		named_type<string>* canvas_info_fields = new named_type<string>("fields");
		named_type<string>* layer_info_field = new named_type<string>("layer-name");


        po::options_description settings(_("Settings"));
        settings.add_options()
			("target,t", target, _("Specify output target (Default:unknown)"))
            ("width,w", width, _("Set the image width (Use zero for file default)"))
            ("height,h", height, _("Set the image height (Use zero for file default)"))
            ("span,s", span, _("Set the diagonal size of image window (Span)"))
            ("antialias,a", antialias, _("Set antialias amount for parametric renderer."))
            ("quality,Q", quality->default_value(DEFAULT_QUALITY), strprintf(_("Specify image quality for accelerated renderer (default=%d)"), DEFAULT_QUALITY).c_str())
            ("gamma,g", gamma->default_value(2.2), _("Gamma"))
            ("threads,T", threads, _("Enable multithreaded renderer using specified # of threads"))
            ("canvas,c", canvas, _("Render the canvas with the given id instead of the root."))
            ("output-file,o", output_file, _("Specify output filename"))
            ("input-file", input_file, _("Specify input filename"))
            ("fps",  fps, _("Set the frame rate"))
			("time",  time, _("Render a single frame at <seconds>"))
			("begin-time",  begin_time, _("Set the starting time"))
			("start-time",  start_time, _("Set the starting time"))
			("end-time",  end_time, _("Set the ending time"))
			("dpi",  dpi, _("Set the physical resolution (dots-per-inch)"))
			("dpi-x",  dpi_x, _("Set the physical X resolution (dots-per-inch)"))
			("dpi-y",  dpi_y, _("Set the physical Y resolution (dots-per-inch)"))
            ;

        po::options_description switchopts(_("Switch options"));
        switchopts.add_options()
            ("verbose,v", po::value<int>(), _("Output verbosity level"))
            ("quiet,q", _("Quiet mode (No progress/time-remaining display)"))
            ("benchmarks,b", _("Print benchmarks"))
            ;

        po::options_description misc(_("Misc options"));
        misc.add_options()
			("append", append_filename, _("Append layers in <filename> to composition"))
            ("canvas-info", canvas_info_fields, _("Print out specified details of the root canvas"))
            ("list-canvases", _("List the exported canvases in the composition"))
            ;

        po::options_description info("Synfig info options");
        info.add_options()
			("help", _("produce a help message"))
            ("importers", _("Print out the list of available importers"))
			("info", _("Print out misc build information"))
            ("layers", _("Print out the list of available layers"))
            ("layer-info", layer_info_field, _("Print out layer's description, parameter info, etc."))
            ("license", _("Print out license information"))
            ("modules", _("Print out the list of loaded modules"))
            ("targets", _("Print out the list of available targets"))
            ("target-video-codecs", _("Print out the list of available target video codecs"))
            ("valuenodes", _("Print out the list of available ValueNodes"))
            ("version", _("Print out version information"))
            ;

#ifdef _DEBUG
        po::options_description debug(_("Synfig debug flags"));
        debug.add_options()
            ("guid-test", _("Test GUID generation"))
			("signal-test", _("Test signal implementation"))
            ;
#endif

        // Declare an options description instance which will include
        // all the options
        po::options_description all("");
        all.add(settings).add(switchopts).add(misc).add(info);

#ifdef _DEBUG
		all.add(debug);
#endif

        // Declare an options description instance which will be shown
        // to the user
        po::options_description visible("");
        visible.add(settings).add(switchopts).add(misc).add(info);


        po::variables_map vm;
        po::store(po::command_line_parser(ac, av).options(all).run(), vm);



        // Switch options ---------------------------------------------
        if (vm.count("verbose"))
        {
			verbosity = vm["verbose"].as<int>();
			VERBOSE_OUT(1) << _("verbosity set to ") << verbosity
						   << endl;
		}

		if (vm.count("benchmarks"))
			print_benchmarks=true;

		if (vm.count("quiet"))
			be_quiet=true;



#ifdef _DEBUG
		// DEBUG options ----------------------------------------------
		if (vm.count("signal-test"))
		{
			signal_test();
			return SYNFIGTOOL_HELP;
		}

		if (vm.count("guid-test"))
		{
			guid_test();
			return SYNFIGTOOL_HELP;
		}
#endif


        // Info options -----------------------------------------------
        if (vm.count("help"))
        {
			print_usage();
            cout << visible;

            return SYNFIGTOOL_HELP;
        }
        
        if (vm.count("info"))
        {
			cout << PACKAGE "-" VERSION << endl;
#ifdef DEVEL_VERSION
				cout << endl << DEVEL_VERSION << endl << endl;
#endif
			cout << "Compiled on " __DATE__ /* " at "__TIME__ */;
#ifdef __GNUC__
			cout << " with GCC " << __VERSION__;
#endif
#ifdef _MSC_VER
			cout << " with Microsoft Visual C++ "
				 << (_MSC_VER>>8) << '.' << (_MSC_VER&255);
#endif
#ifdef __TCPLUSPLUS__
			cout << " with Borland Turbo C++ "
				 << (__TCPLUSPLUS__>>8) << '.'
				 << ((__TCPLUSPLUS__&255)>>4) << '.'
				 << (__TCPLUSPLUS__&15);
#endif
			cout << endl << SYNFIG_COPYRIGHT << endl;
			cout << endl;

			return SYNFIGTOOL_HELP;
		}
		
		if (vm.count("version"))
		{
			cerr << PACKAGE << " " << VERSION << endl;

			return SYNFIGTOOL_HELP;
		}

		if (vm.count("license"))
		{
			cerr << PACKAGE << " " << VERSION << endl;
			cout << SYNFIG_COPYRIGHT << endl << endl;
			cerr << SYNFIG_LICENSE << endl << endl;

			return SYNFIGTOOL_HELP;
		}

		if (vm.count("target-video-codecs"))
		{
			print_target_video_codecs_help();

			return SYNFIGTOOL_HELP;
		}

		// TODO: Optional load of main only if needed. i.e. not needed to display help
		// Synfig Main initialization needs to be after verbose and
		// before any other where it's used
		Progress p(progname);
		synfig::Main synfig_main(dirname(progname), &p);

		if (vm.count("layers"))
		{
			synfig::Layer::Book::iterator iter =
				synfig::Layer::book().begin();
			for(; iter != synfig::Layer::book().end(); iter++)
				if (iter->second.category != CATEGORY_DO_NOT_USE)
					cout << iter->first << endl;

			return SYNFIGTOOL_HELP;
		}
		
		if (vm.count("layer-info"))
		{
			Layer::Handle layer =
				synfig::Layer::create(vm["layer-info"].as<string>());

			cout << "Layer Name: " << layer->get_name() << endl;
			cout << "Localized Layer Name: " << layer->get_local_name() << endl;
			cout << "Version: " << layer->get_version() << endl;

			Layer::Vocab vocab = layer->get_param_vocab();
			for(; !vocab.empty(); vocab.pop_front())
			{
				cout << "param - " << vocab.front().get_name();
				if(!vocab.front().get_critical())
					cout << " (not critical)";
				cout << endl << "\tLocalized Name: "
					 << vocab.front().get_local_name() << endl;

				if(!vocab.front().get_description().empty())
					cout << "\tDescription: "
						 << vocab.front().get_description() << endl;

				if(!vocab.front().get_hint().empty())
					cout << "\tHint: "
						 << vocab.front().get_hint() << endl;
			}

			return SYNFIGTOOL_HELP;
		}
		
		if (vm.count("modules"))
		{
			synfig::Module::Book::iterator iter =
				synfig::Module::book().begin();
			for(; iter != synfig::Module::book().end(); iter++)
				cout << iter->first << endl;

			return SYNFIGTOOL_HELP;
		}

		if (vm.count("targets"))
		{
			synfig::Target::Book::iterator iter =
				synfig::Target::book().begin();
			for(; iter != synfig::Target::book().end(); iter++)
				cout << iter->first << endl;

			return SYNFIGTOOL_HELP;
		}

		if (vm.count("valuenodes"))
		{
			synfig::LinkableValueNode::Book::iterator iter =
				synfig::LinkableValueNode::book().begin();
			for(; iter != synfig::LinkableValueNode::book().end(); iter++)
				cout << iter->first << endl;

			return SYNFIGTOOL_HELP;
		}

		if (vm.count("importers"))
		{
			synfig::Importer::Book::iterator iter =
				synfig::Importer::book().begin();
			for(; iter != synfig::Importer::book().end(); iter++)
				cout << iter->first << endl;

			return SYNFIGTOOL_HELP;
		}

		//FIXME: append has to be before list-canvases

		if (vm.count("list-canvases"))
		{
			Job job;
			int ret;
			ret = job.load_file(vm["input-file"].as<string>());
			
			if (ret != SYNFIGTOOL_OK)
				return ret;

			print_child_canvases(job.filename + "#",job.root);

			cerr << endl;

			return SYNFIGTOOL_OK;
		}
		
		if (vm.count("canvas-info"))
		{
			Job job;
			int ret;
			ret = job.load_file(vm["input-file"].as<string>());
			
			if (ret != SYNFIGTOOL_OK)
				return ret;

			extract_canvas_info(job, vm["canvas-info"].as<string>());
			print_canvas_info(job);
			
			return SYNFIGTOOL_OK;
		}

		// Setup Job list ----------------------------------------------
		
		list<Job> job_list;
		
		// ...
		
		// Process Job list --------------------------------------------
		
		if(!job_list.size())
		{
			cerr << _("Nothing to do!") << endl;
			return SYNFIGTOOL_BORED;
		}

		for(; job_list.size(); job_list.pop_front())
		{
			VERBOSE_OUT(3) << job_list.front().filename << " -- " << endl;
			VERBOSE_OUT(3) << '\t'
						   <<
				strprintf("w:%d, h:%d, a:%d, pxaspect:%f, imaspect:%f, span:%f",
					job_list.front().desc.get_w(),
					job_list.front().desc.get_h(),
					job_list.front().desc.get_antialias(),
					job_list.front().desc.get_pixel_aspect(),
					job_list.front().desc.get_image_aspect(),
					job_list.front().desc.get_span()
					)
				<< endl;

			VERBOSE_OUT(3) << '\t'
						   <<
				strprintf("tl:[%f,%f], br:[%f,%f], focus:[%f,%f]",
					job_list.front().desc.get_tl()[0],
					job_list.front().desc.get_tl()[1],
					job_list.front().desc.get_br()[0],
					job_list.front().desc.get_br()[1],
					job_list.front().desc.get_focus()[0],
					job_list.front().desc.get_focus()[1]
					)
					<< endl;

			RenderProgress p;
			p.task(job_list.front().filename + " ==> " +
				   job_list.front().outfilename);
			if(job_list.front().sifout)
			{
				if(!save_canvas(job_list.front().outfilename,
								job_list.front().canvas))
				{
					cerr << _("Render Failure.") << endl;
					return SYNFIGTOOL_RENDERFAILURE;
				}
			}
			else
			{
				VERBOSE_OUT(1) << _("Rendering...") << endl;
				etl::clock timer;
				timer.reset();

				// Call the render member of the target
				if(!job_list.front().target->render(&p))
				{
					cerr << _("Render Failure.") << endl;
					return SYNFIGTOOL_RENDERFAILURE;
				}

				if(print_benchmarks)
					cout << job_list.front().filename
						 << _(": Rendered in ") << timer()
						 << _(" seconds.") << endl;
			}
		}

		job_list.clear();

		VERBOSE_OUT(1) << _("Done.") << endl;
		
		return SYNFIGTOOL_OK;

    }
    catch(std::exception& e) {
        cout << e.what() << "\n";
    }
}
