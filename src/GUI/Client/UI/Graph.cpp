// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

// Qt header
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSvgGenerator>
#include <QToolBar>
#include <QToolButton>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleValidator>
#include <QGroupBox>
#include <QFileIconProvider>

// Qwt headers
#include "qwt/qwt_picker.h"
#include "qwt/qwt_plot_canvas.h"
#include "qwt/qwt_plot_panner.h"
#include "qwt/qwt_plot_zoomer.h"
#include "qwt/qwt_double_rect.h"
#include "qwt/qwt_scale_widget.h"
#include "qwt/qwt_polygon.h"

//header
#include "GUI/Client/Core/NPlotXY.hpp"
#include "GUI/Client/Core/NLog.hpp"
#include "GUI/Client/UI/PixMaps.hpp"
#include "GUI/Client/UI/BodePlot.hpp"
#include "GUI/Client/UI/Graph.hpp"

using namespace CF::GUI::ClientCore;

////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace GUI {
namespace ClientUI {

////////////////////////////////////////////////////////////////////////////////

  //class that manage qwt zoom
  class Zoomer: public QwtPlotZoomer
  {
  public:
    Zoomer(int xAxis, int yAxis, QwtPlotCanvas *canvas):
        QwtPlotZoomer(xAxis, yAxis, canvas)
    {
      //if draged the mouse, get a canvas, if double clicking
      //use the whole curent view as canvas
      setSelectionFlags(QwtPicker::DragSelection | QwtPicker::CornerToCorner);
      setTrackerMode(QwtPicker::AlwaysOff);
      setRubberBand(QwtPicker::NoRubberBand);

      // RightButton: zoom out by 1
      // Ctrl+RightButton: zoom out to full size
      setMousePattern(QwtEventPattern::MouseSelect2,
                      Qt::RightButton, Qt::ControlModifier);

      setMousePattern(QwtEventPattern::MouseSelect3,
                      Qt::RightButton);
    }
  };


  Graph::Graph(QWidget *parent) :
      QWidget(parent)
  {

    ////creation phase

    //creat a grid layout
    QGridLayout * layout_grid = new QGridLayout();

    //creat layout
    QHBoxLayout * layout_h = new QHBoxLayout();
    QHBoxLayout * layout_h2 = new QHBoxLayout();
    QHBoxLayout * layout_h3 = new QHBoxLayout();

    m_layout_zoom = new QHBoxLayout();
    QHBoxLayout * layout_zoom_option_point = new QHBoxLayout();
    QHBoxLayout * layout_zoom_option_size = new QHBoxLayout();

    QVBoxLayout * layout_zoom_option = new QVBoxLayout();

    QHBoxLayout * layout_option = new QHBoxLayout();

    //create group box
    QGroupBox * m_scale_box = new QGroupBox("Scale");
    m_scale_box->setCheckable(true);

    QGroupBox * m_options_box = new QGroupBox("Options");
    m_options_box->setCheckable(true);

    //creat the BodePlot
    m_plot = new BodePlot(this,true);
    m_plot->setMargin(5);

    //create the toolbar that contain the widget's button
    QToolBar * tool_bar = new QToolBar(this);

    //cearte a zoom button

    QToolButton * btn_zoom = new QToolButton(tool_bar);
    btn_zoom->setText("Zoom");
    btn_zoom->setIcon(QIcon(zoom_xpm));
    btn_zoom->setCheckable(true);
    btn_zoom->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);



    //create a SVG save button
    QToolButton * btn_svg = new QToolButton(tool_bar);
    btn_svg->setText("SVG");
    btn_svg->setIcon(QIcon(print_xpm));
    btn_svg->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);


    //create a text save button
    QToolButton * btn_txt = new QToolButton(tool_bar);
    btn_txt->setText("TXT");
    QFileIconProvider txt_icone;
    btn_txt->setIcon(txt_icone.icon(QFileIconProvider::File));
    btn_txt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);


    //creating label to display information
    m_label_bottom = new QLabel(this);

    //set scale inline system
    m_line_min_x = new QLineEdit();
    m_line_min_x->setValidator(new QDoubleValidator(nullptr));
    m_line_max_x = new QLineEdit();
    m_line_max_x->setValidator(new QDoubleValidator(nullptr));
    m_line_min_y = new QLineEdit();
    m_line_min_y->setValidator(new QDoubleValidator(nullptr));
    m_line_max_y = new QLineEdit();
    m_line_max_y->setValidator(new QDoubleValidator(nullptr));
    m_button_set_scale = new QPushButton("Set scale");

    ////Placment and initialisation

    //assign the grid layout to this widget
    this->setLayout(layout_grid);

    //adding 3 horizontal layout to grid layout
    layout_grid->addLayout(layout_h,0,0,1,1);
    layout_grid->addLayout(layout_h2,1,0,1,1);
    layout_grid->addLayout(layout_h3,2,0,1,1);
    layout_grid->addWidget(m_scale_box,3,0,1,1);
    layout_grid->addWidget(m_options_box,4,0,1,1);

    setContextMenuPolicy(Qt::NoContextMenu);

    //initialising the zoom mode
    m_zoomer = new Zoomer( QwtPlot::xBottom, QwtPlot::yLeft,
                              m_plot->canvas());
    m_zoomer->setRubberBand(QwtPicker::RectRubberBand);
    m_zoomer->setRubberBandPen(QColor(Qt::green));
    m_zoomer->setTrackerMode(QwtPicker::ActiveOnly);
    m_zoomer->setTrackerPen(QColor(Qt::white));


    //assign the canvas to the panner
    m_panner = new QwtPlotPanner(m_plot->canvas());
    m_panner->setMouseButton(Qt::MidButton);
    m_panner->setEnabled(true);

    //set the mouse over canvas style
    m_picker = new QwtPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft,
                                 QwtPicker::PointSelection | QwtPicker::DragSelection,
                                 QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn,
                                 m_plot->canvas());
    m_picker->setRubberBandPen(QColor(Qt::green));
    m_picker->setRubberBand(QwtPicker::CrossRubberBand);
    m_picker->setTrackerPen(QColor(Qt::black));

    //adding buttons to the toolbar
    tool_bar->addWidget(btn_zoom);
    tool_bar->addWidget(btn_svg);
    tool_bar->addWidget(btn_txt);

    //set the mouse to not be used for zoom
    enable_zoom_mode(false);

    //display first information ( default information )
    show_info();

    //adding widget to layout to make the composant visible and well placed
    layout_h->addWidget(tool_bar);
    layout_h2->addWidget(m_plot);
    layout_h3->addWidget(m_label_bottom);

    m_label_min_x = new QLabel("x min");
    m_label_max_x = new QLabel("x max");
    m_label_min_y = new QLabel("y min");
    m_label_max_y = new QLabel("y max");
    layout_zoom_option_point->addWidget(m_label_min_x);
    layout_zoom_option_point->addWidget(m_line_min_x);
    layout_zoom_option_point->addWidget(m_label_max_x);
    layout_zoom_option_point->addWidget(m_line_max_x);
    layout_zoom_option_size->addWidget(m_label_min_y);
    layout_zoom_option_size->addWidget(m_line_min_y);
    layout_zoom_option_size->addWidget(m_label_max_y);
    layout_zoom_option_size->addWidget(m_line_max_y);

    layout_zoom_option->addLayout(layout_zoom_option_point);
    layout_zoom_option->addLayout(layout_zoom_option_size);

    m_layout_zoom->addLayout(layout_zoom_option);
    m_layout_zoom->addWidget(m_button_set_scale);

    NPlotXY::PlotDataPtr plot_data( new NPlotXY::PlotData() );
    std::vector<QString> vector_temp2(0);
    graph_option = new GraphOption(plot_data,vector_temp2,m_plot,this);

    layout_option->addWidget(graph_option);

    m_scale_box->setLayout(m_layout_zoom);
    m_options_box->setLayout(layout_option);

    ////Conncetion phase
    connect(btn_svg, SIGNAL(clicked()), SLOT(export_svg()));
    connect(btn_txt, SIGNAL(clicked()),graph_option, SLOT(save_functions()));
    connect(btn_zoom, SIGNAL(toggled(bool)), SLOT(enable_zoom_mode(bool)));
    connect(m_picker, SIGNAL(moved(const QPoint &)),
            SLOT(moved(const QPoint &)));
    connect(m_picker, SIGNAL(selected(const QwtPolygon &)),
            SLOT(selected(const QwtPolygon &)));
    connect(m_button_set_scale, SIGNAL(clicked()), SLOT(set_scale()));
    connect(m_line_min_x, SIGNAL(editingFinished()), SLOT(set_scale()));
    connect(m_line_max_x, SIGNAL(editingFinished()), SLOT(set_scale()));
    connect(m_line_min_y, SIGNAL(editingFinished()), SLOT(set_scale()));
    connect(m_line_max_y, SIGNAL(editingFinished()), SLOT(set_scale()));

    connect (m_options_box, SIGNAL(toggled(bool)), this, SLOT (show_graph_option(bool)));
    connect (m_scale_box, SIGNAL(toggled(bool)), this, SLOT (show_scale_option(bool)));


    ////Connection Boost
    NPlotXYNotifier::instance().notify_history.connect(
        boost::bind(&Graph::set_xy_data, this, _1, _2) );

  }

  Graph::~Graph(){
  }

  void Graph::export_svg()
  {
    //default file name
    QString file_name = "bode.svg";

    //getting the file name and path for saving file
    file_name = QFileDialog::getSaveFileName(
        this, "Export File Name", QString(),
        "SVG Documents (*.svg)");

    if ( !file_name.isEmpty() )
    {
      //create generator with file spec
      QSvgGenerator generator;
      generator.setFileName(file_name);
      generator.setSize(QSize(800, 600));
      m_plot->print(generator);
    }
  }


  void Graph::enable_zoom_mode(bool on)
  {
    m_zoomer->setEnabled(on);
    //unComment to set the originale scale on enable or disable zoom
    //m_zoomer->zoom(0);

    m_picker->setEnabled(!on);

    show_info();
  }


  void Graph::show_info(QString text)
  {

    if ( text == QString::null )
    {
      if ( m_picker->isEnabled() )
      {
        text = "Cursor Pos: Press left mouse button in plot region";
      }
      else
      {
        text = "Zoom: Press left mouse button and drag, unZoom: Press right  mouse button, fullunZoom: Press CTRL + right  mouse button";
      }
    }

    m_label_bottom->setText(text);
  }


  void Graph::moved(const QPoint &pos)
  {
    QString info;
    info.sprintf("X=%g, Y=%g",
        m_plot->invTransform(QwtPlot::xBottom, pos.x()),
        m_plot->invTransform(QwtPlot::yLeft, pos.y())
    );
    show_info(info);
  }

  void Graph::selected(const QwtPolygon & poly)
  {
    show_info();
  }

  void Graph::set_xy_data(NPlotXY::PlotDataPtr & fcts,
                          std::vector<QString> & fct_label){

    cf_assert( is_not_null(m_plot) );
    graph_option->set_data(fcts,fct_label);

    show_info();
  }

  void Graph::set_scale(){

    double x,y,weight,height;

    if(m_line_min_x->text().isEmpty())
    {
      x = m_plot->axisScaleDiv(QwtPlot::xBottom)->lowerBound();
    }else{
      x = m_line_min_x->text().toDouble();
    }

    if(m_line_max_x->text().isEmpty())
    {
      weight = m_plot->axisScaleDiv(QwtPlot::xBottom)->upperBound();
    }else{
      weight = m_line_max_x->text().toDouble();
    }


    if(m_line_min_y->text().isEmpty())
    {
      y = m_plot->axisScaleDiv(QwtPlot::yLeft)->lowerBound();
    }else{
      y = m_line_min_y->text().toDouble();
    }

    if(m_line_max_y->text().isEmpty())
    {
      height = m_plot->axisScaleDiv(QwtPlot::yLeft)->upperBound();
    }else{
      height = m_line_max_y->text().toDouble();
    }

    m_plot->setAxisScale(QwtPlot::xBottom,x,weight);
    m_plot->setAxisScale(QwtPlot::yLeft,y,height);


    m_zoomer->setZoomBase();

    m_plot->replot();

     show_info();
  }

  void Graph::show_graph_option(bool visible)
  {
    graph_option->setVisible(visible);
  }

  void Graph::show_scale_option(bool visible)
  {

    m_label_min_x->setVisible(visible);
    m_label_max_x->setVisible(visible);
    m_label_min_y->setVisible(visible);
    m_label_max_y->setVisible(visible);

    m_line_min_x->setVisible(visible);
    m_line_max_x->setVisible(visible);
    m_line_min_y->setVisible(visible);
    m_line_max_y->setVisible(visible);

    m_button_set_scale->setVisible(visible);
  }

  void  Graph::reset_base_zoom()
  {
      m_zoomer->setZoomBase();
      m_plot->replot();
  }




////////////////////////////////////////////////////////////////////////////////

} // ClientUI
} // GUI
} // CF

