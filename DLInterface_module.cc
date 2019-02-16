////////////////////////////////////////////////////////////////////////
// Class:       DLInterface
// Plugin Type: producer (art v2_05_01)
// File:        DLInterface_module.cc
//
// Generated at Wed Aug 29 05:49:09 2018 by Taritree,,, using cetskelgen
// from cetlib version v1_21_00.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>

// larsoft
#include "lardataobj/RecoBase/OpFlash.h"

// ubcv
#include "ubcv/LArCVImageMaker/LArCVSuperaDriver.h"
#include "ubcv/LArCVImageMaker/ImageMetaMaker.h"
#include "ubcv/LArCVImageMaker/SuperaMetaMaker.h"
#include "ubcv/LArCVImageMaker/SuperaWire.h"
#include "ubcv/LArCVImageMaker/LAr2Image.h"

// larcv
#include "larcv/core/Base/larcv_base.h"
#include "larcv/core/Base/PSet.h"
#include "larcv/core/Base/LArCVBaseUtilFunc.h"
#include "larcv/core/DataFormat/EventImage2D.h"
#include "larcv/core/DataFormat/Image2D.h"
#include "larcv/core/DataFormat/ImageMeta.h"
#include "larcv/core/DataFormat/ROI.h"
#include "larcv/core/json/json_utils.h"

// larlite
#include "DataFormat/opflash.h"

// ublarcvapp
#include "ublarcvapp/UBImageMod/UBSplitDetector.h"
#include "ublarcvapp/ubdllee/FixedCROIFromFlashAlgo.h"
#include "ublarcvapp/ubdllee/FixedCROIFromFlashConfig.h"

// ROOT
#include "TMessage.h"

// zmq-c++
#include "zmq.hpp"

#ifdef HAS_TORCH
// TORCH
#include <torch/script.h>
#include <torch/torch.h>

// larcv torch utils
#include "larcv/core/TorchUtil/TorchUtils.h"
#endif

class DLInterface;


class DLInterface : public art::EDProducer, larcv::larcv_base {
public:
  explicit DLInterface(fhicl::ParameterSet const & p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  typedef enum { kServer=0, kPyTorchCPU, kTensorFlowCPU, kDummyServer } NetInterface_t;
  typedef enum { kWholeImageSplitter=0, kFlashCROI } Cropper_t;

  // Plugins should not be copied or assigned.
  DLInterface(DLInterface const &) = delete;
  DLInterface(DLInterface &&) = delete;
  DLInterface & operator = (DLInterface const &) = delete;
  DLInterface & operator = (DLInterface &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;
  void beginJob() override;
  void endJob() override;
  
private:

  // Declare member data here.

  // supera/image formation
  larcv::LArCVSuperaDriver _supera;  
  std::string _wire_producer_name;
  std::string _supera_config;
  int runSupera( art::Event& e, std::vector<larcv::Image2D>& wholeview_imgs );

  // image processing/splitting/cropping
  Cropper_t   _cropping_method;
  std::string _cropping_method_name;
  ublarcvapp::UBSplitDetector _imagesplitter; //< full splitter
  ublarcvapp::ubdllee::FixedCROIFromFlashAlgo _croifromflashalgo; //< croi from flash
  bool        _save_detsplit_input;
  bool        _save_detsplit_output;
  std::string _opflash_producer_name;
  std::vector<larcv::Image2D> _splitimg_v;              //< container holding split images
  int runWholeViewSplitter( const std::vector<larcv::Image2D>& wholeview_v, 
			    std::vector<larcv::Image2D>& splitimg_v,
			    std::vector<larcv::ROI>&     splitroi_v );
  int runCROIfromFlash( const std::vector<larcv::Image2D>& wholeview_v, 
			art::Event& e,
			std::vector<larcv::Image2D>& splitimg_v,
			std::vector<larcv::ROI>&     splitroi_v );
  
  // the network interface to run
  NetInterface_t _interface;

  // interface: pytorch cpu
  std::string _pytorch_net_script;
#ifdef HAS_TORCH
  std::shared_ptr<torch::jit::script::Module> _module;  //< pointer to pytorch network
#endif
  void loadNetwork_PyTorchCPU();
  int runPyTorchCPU( const std::vector<larcv::Image2D>& wholeview_v,
		     std::vector<larcv::Image2D>& splitimg_v, 
		     std::vector<larcv::ROI>& splitroi_v, 
		     std::vector<larcv::Image2D>& showerout_v,
		     std::vector<larcv::Image2D>& trackout_v);

  

  // interface: server (common)
  zmq::context_t* _context;
  zmq::socket_t*  _socket; 
  void openServerSocket();
  void closeServerSocket();
  size_t serializeImages( const size_t nplanes,
			  const std::vector<larcv::Image2D>& img_v,   
			  std::vector< std::vector<unsigned char> >& msg_img_v,
			  std::vector<std::string>& msg_meta_v,
			  std::vector<std::string>& msg_name_v );


  // interface: ssnet gpu server
  int runSSNetServer( const std::vector<larcv::Image2D>& wholeview_v, 
		      std::vector<larcv::Image2D>& splitimg_v, 
		      std::vector<larcv::ROI>& splitroi_v, 
		      std::vector<larcv::Image2D>& showerout_v,
		      std::vector<larcv::Image2D>& trackout_v );

  // interface: dummy server (for debug)
  int runDummyServer( art::Event& e );
  void sendDummyMessage();

  // merger/post-processing
  void mergeSSNetOutput( const std::vector<larcv::Image2D>& wholeview_v, 
			 const std::vector<larcv::ROI>& splitroi_v, 
			 const std::vector<larcv::Image2D>& showerout_v,
			 const std::vector<larcv::Image2D>& trackout_v,
			 std::vector<larcv::Image2D>& showermerged_v,
			 std::vector<larcv::Image2D>& trackmerged_v );
  

};

/**
 * constructor: configuration module
 *
 */
DLInterface::DLInterface(fhicl::ParameterSet const & p)
  : larcv::larcv_base("DLInterface_module")
    // Initialize member data here.
{
  // Call appropriate produces<>() functions here.
  _wire_producer_name = p.get<std::string>("WireProducerName");
  _supera_config      = p.get<std::string>("SuperaConfigFile");
  std::string inter   = p.get<std::string>("NetInterface");
  if ( inter=="Server" )
    _interface = kServer;
  else if ( inter=="PyTorchCPU" )
    _interface = kPyTorchCPU;
  else if ( inter=="TensorFlowCPU" )
    _interface = kTensorFlowCPU;
  else if ( inter=="DummyServer" ) // for debugging
    _interface = kDummyServer;
  else {
    throw cet::exception("DLInterface") << "unrecognized network interface, " << inter << ". "
					<< "choices: { Server, Pytorch, TensorFlow }"
					<< std::endl;
  }

  if ( _interface==kTensorFlowCPU )
    throw cet::exception("DLInterface") << "TensorFlowCPU interface not yet implemented" << std::endl;

  // Image cropping/splitting config
  _cropping_method_name = p.get<std::string>("CroppingMethod");
  if ( _cropping_method_name=="WholeImageSplitter" )
    _cropping_method = kWholeImageSplitter;
  else if ( _cropping_method_name=="FlashCROI" )
    _cropping_method = kFlashCROI;
  else {
    throw cet::exception("DLInterface") << "invalid cropping method. "
					<< "options: {\"WholeImageSplitter\",\"FlashCROI\"}" 
					<< std::endl;
  }
  _save_detsplit_input  = p.get<bool>("SaveDetsplitImagesInput");
  _save_detsplit_output = p.get<bool>("SaveNetOutSplitImagesOutput");
  if ( _cropping_method==kFlashCROI ) {
    _opflash_producer_name = p.get<std::string>("OpFlashProducerName");
  }


  std::string supera_cfg;
  cet::search_path finder("FHICL_FILE_PATH");
  if( !finder.find_file(_supera_config, supera_cfg) )
    throw cet::exception("DLInterface") << "Unable to find supera cfg in "  << finder.to_string() << "\n";

  // check cfg content top level
  larcv::PSet main_cfg = larcv::CreatePSetFromFile(_supera_config).get<larcv::PSet>("ProcessDriver");

  // get list of processors
  std::vector<std::string> process_names = main_cfg.get< std::vector<std::string> >("ProcessName");
  std::vector<std::string> process_types = main_cfg.get< std::vector<std::string> >("ProcessType");
  larcv::PSet              process_list  = main_cfg.get<larcv::PSet>("ProcessList");
  larcv::PSet              split_cfg     = main_cfg.get<larcv::PSet>("UBSplitConfig"); 
  
  if ( process_names.size()!=process_types.size() ) {
    throw std::runtime_error( "Number of Supera ProcessName(s) and ProcessTypes(s) is not the same." );
  }

  // configure supera (convert art::Event::CalMod -> Image2D)
  _supera.configure(_supera_config);

  // configure image splitter (fullimage into detsplit image)
  _imagesplitter.configure( split_cfg );

  // get the path to the saved ssnet
  _pytorch_net_script = p.get<std::string>("PyTorchNetScript");


  // verbosity
  int verbosity = p.get<int>("Verbosity",2);
  set_verbosity( (::larcv::msg::Level_t) verbosity );
  _supera.set_verbosity( (::larcv::msg::Level_t) verbosity );

}

/** 
 * produce DL network products
 *
 */
void DLInterface::produce(art::Event & e)
{

  // get the wholeview images for the network
  std::vector<larcv::Image2D> wholeview_v;
  int nwholeview_imgs = runSupera( e, wholeview_v );
  std::cout << "number of wholeview images: " << nwholeview_imgs << std::endl;

  // we often have to pre-process the image, e.g. split it.
  // eventually have options here. But for now, wholeview splitter
  std::vector<larcv::Image2D> splitimg_v;
  std::vector<larcv::ROI>     splitroi_v;

  
  int nsplit_imgs = 0;

  switch (_cropping_method) {
  case kWholeImageSplitter:
    nsplit_imgs = runWholeViewSplitter( wholeview_v, splitimg_v, splitroi_v );
    break;
  case kFlashCROI:
    nsplit_imgs = runCROIfromFlash( wholeview_v, e, splitimg_v, splitroi_v );
    break;
  }
  std::cout << "number of split images: " << nsplit_imgs << std::endl;

  // containers for outputs
  std::vector<larcv::Image2D> showerout_v;
  std::vector<larcv::Image2D> trackout_v;

  // run network (or not)

  int status = 0;
  switch (_interface) {
  case kDummyServer:
  case kTensorFlowCPU:
    status = runDummyServer(e);
    break;
  case kPyTorchCPU:
    status = runPyTorchCPU( wholeview_v, splitimg_v, splitroi_v, showerout_v, trackout_v );
    break;
  case kServer:
    status = runSSNetServer( wholeview_v, splitimg_v, splitroi_v, showerout_v, trackout_v );
    break;
  }

  if ( status!=0 )
    throw cet::exception("DLInterface") << "Error running network" << std::endl;

  // merge the output
  std::vector<larcv::Image2D> showermerged_v;
  std::vector<larcv::Image2D> trackmerged_v;
  mergeSSNetOutput( wholeview_v, splitroi_v, showerout_v, trackout_v, showermerged_v, trackmerged_v );

  // prepare the output
  larcv::IOManager& io = _supera.driver().io_mutable();
  
  // save the wholeview images back to the supera IO
  larcv::EventImage2D* ev_imgs  = (larcv::EventImage2D*) io.get_data( larcv::kProductImage2D, "wire" );
  std::cout << "wire eventimage2d=" << ev_imgs << std::endl;
  ev_imgs->Emplace( std::move(wholeview_v) );

  // save detsplit input
  larcv::EventImage2D* ev_splitdet = nullptr;
  if ( _save_detsplit_input ) {
    ev_splitdet = (larcv::EventImage2D*) io.get_data( larcv::kProductImage2D, "detsplit" );
    ev_splitdet->Emplace( std::move(splitimg_v) );
  }

  // save detsplit output
  larcv::EventImage2D* ev_netout_split[2] = {nullptr,nullptr};
  if ( _save_detsplit_output ) {
    ev_netout_split[0] = (larcv::EventImage2D*) io.get_data( larcv::kProductImage2D, "netoutsplit_shower" );
    ev_netout_split[1] = (larcv::EventImage2D*) io.get_data( larcv::kProductImage2D, "netoutsplit_track" );
    ev_netout_split[0]->Emplace( std::move(showerout_v) );
    ev_netout_split[1]->Emplace( std::move(trackout_v) );
  }

  // save merged out
  larcv::EventImage2D* ev_merged[2] = {nullptr,nullptr};
  ev_merged[0] = (larcv::EventImage2D*)io.get_data( larcv::kProductImage2D, "ssnetshower" );
  ev_merged[1] = (larcv::EventImage2D*)io.get_data( larcv::kProductImage2D, "ssnettrack" );
  ev_merged[0]->Emplace( std::move(showermerged_v) );
  ev_merged[1]->Emplace( std::move(trackmerged_v) );

  // save entry
  std::cout << "saving entry" << std::endl;
  io.save_entry();
  
  // we clear entries ourselves
  std::cout << "clearing entry" << std::endl;
  io.clear_entry();
  
}

int DLInterface::runDummyServer( art::Event& e ) {
  sendDummyMessage();
  return 0;
}

/**
 * have supera make larcv images
 *
 * get recob::Wire from art::Event, pass it to supera
 * the _supera object will contain the larcv IOManager which will have images stored
 *
 * @param[in] e art::Event with wire data
 * @return int number of images to process by network
 *
 */
int DLInterface::runSupera( art::Event& e, std::vector<larcv::Image2D>& wholeview_imgs ) {

  //
  // set data product
  // 
  // :wire  
  art::Handle<std::vector<recob::Wire> > data_h;
  //  handle sub-name
  if(_wire_producer_name.find(" ")<_wire_producer_name.size()) {
    e.getByLabel(_wire_producer_name.substr(0,_wire_producer_name.find(" ")),
		 _wire_producer_name.substr(_wire_producer_name.find(" ")+1,_wire_producer_name.size()-_wire_producer_name.find(" ")-1),
		 data_h);
  }else{ e.getByLabel(_wire_producer_name, data_h); }
  if(!data_h.isValid()) { 
    std::cerr<< "Attempted to load recob::Wire data: " << _wire_producer_name << std::endl;
    throw ::cet::exception("DLInterface") << "Could not locate recob::Wire data!" << std::endl;
  }
  _supera.SetDataPointer(*data_h,_wire_producer_name);

  // execute supera
  bool autosave_entry = false;
  std::cout << "process event: (" << e.id().run() << "," << e.id().subRun() << "," << e.id().event() << ")" << std::endl;
  _supera.process(e.id().run(),e.id().subRun(),e.id().event(), autosave_entry);

  // get the images
  auto ev_imgs  = (larcv::EventImage2D*) _supera.driver().io_mutable().get_data( larcv::kProductImage2D, "wire" );
  ev_imgs->Move( wholeview_imgs );
  
  return wholeview_imgs.size();
}

/**
 * take wholeview images (one per plane in vector) and turn into subimage crops (many per plane in vector)
 *
 * passes the images into the UBSplitDetector object (_imagesplitter)
 *
 * @param[in] wholeview_v vector of whole-view images. one per plane.
 * @param[inout] splitimg_v vector of cropped images. many per plane.
 * @param[inout] splitroi_v vector ROIs for split images. many  per plane.
 *
 **/
int DLInterface::runWholeViewSplitter( const std::vector<larcv::Image2D>& wholeview_v, 
				       std::vector<larcv::Image2D>& splitimg_v,
				       std::vector<larcv::ROI>&     splitroi_v ) {
  
  // //
  // // define the image meta and image
  // //
  // std::vector<larcv::ImageMeta> meta_v;
  // for ( auto const& img : wholeview_v ) {
  //   meta_v.push_back( img.meta() );
  // }
  
  //
  // debug: dump the meta definitions for the wholeview images
  std::cout << "DLInterface: superawire images made " << wholeview_v.size() << std::endl;
  for (auto& img : wholeview_v ) {
    std::cout << img.meta().dump() << std::endl;
  }  
  
  //
  // split image into subregions
  //
  splitimg_v.clear();
  splitroi_v.clear();
  try {
    _imagesplitter.process( wholeview_v, splitimg_v, splitroi_v );
  }
  catch (std::exception& e ) {
    throw cet::exception("DLInterface") << "error splitting image: " << e.what() << std::endl;
  }

  return splitimg_v.size();

}

/**
 * produce subimage crops based on the location of the in-time flash
 *
 * passes the images into an instance of FixedCROIFromFlashAlgo (_croifromflashalgo member)
 *
 * @param[in] wholeview_v vector of whole-view images. one per plane.
 * @param[in] e art::Event from which we will get the opflash objects
 * @param[inout] splitimg_v vector of cropped images. many per plane.
 * @param[inout] splitroi_v vector ROIs for split images. many  per plane.
 *
 **/
int DLInterface::runCROIfromFlash( const std::vector<larcv::Image2D>& wholeview_v, 
				   art::Event& e,
				   std::vector<larcv::Image2D>& splitimg_v,
				   std::vector<larcv::ROI>&     splitroi_v ) {

  // first get the opflash objects from art
  art::Handle<std::vector<recob::OpFlash> > data_h;
  //  handle sub-name
  if(_opflash_producer_name.find(" ")<_opflash_producer_name.size()) {
    e.getByLabel(_opflash_producer_name.substr(0,_opflash_producer_name.find(" ")),
		 _opflash_producer_name.substr(_opflash_producer_name.find(" ")+1,_opflash_producer_name.size()-_opflash_producer_name.find(" ")-1),
		 data_h);
  }else{ e.getByLabel(_opflash_producer_name, data_h); }

  if(!data_h.isValid()) { 
    std::cerr<< "Attempted to load recob::Opflash data: " << _opflash_producer_name << std::endl;
    throw cet::exception("DLInterface") << "Could not locate recob::Opflash data!" << std::endl;
  }
  std::cout << "Loaded opflash data" << std::endl;

  const float usec_min = 190*0.015625;
  const float usec_max = 320*0.015625;

  std::vector<larlite::opflash> intime_flashes_v;
  for ( auto const& artflash : *data_h ) {

    if ( artflash.Time()<usec_min || artflash.Time()>usec_max ) continue;

    // hmm, there is no getter for the number of entries in the PE vector. 
    // that seems wrong. we cheat about the number of PEs for now
    // next we have to make larlite objects
    std::vector<double> pe_v(32,0.0);
    for ( size_t ich=0; ich<32; ich++ ) {
      pe_v[ich] = artflash.PE(ich);
    }
    larlite::opflash llflash( artflash.Time(),    artflash.TimeWidth(),
			      artflash.AbsTime(), artflash.Frame(),
			      pe_v,
			      1, 1, 1,
			      artflash.YCenter(), artflash.YWidth(),
			      artflash.ZCenter(), artflash.ZWidth(),
			      artflash.WireCenters(),
			      artflash.WireWidths() );
    intime_flashes_v.emplace_back( std::move(llflash) );
  }
  std::cout << "converted into larlite opflash" << std::endl;

  // pass them to the algo to get CROI    
  for ( auto& llflash : intime_flashes_v ) {
    std::vector<larcv::ROI> flashrois = _croifromflashalgo.findCROIfromFlash( llflash );
    for ( auto& roi : flashrois )
      splitroi_v.emplace_back( std::move(roi) );
  }
  std::cout << "create roi from flash" << std::endl;

  // cropout the regions
  size_t nplanes = wholeview_v.size();
  size_t ncrops = 0;
  for (size_t plane=0; plane<nplanes; plane++ ) {
    const larcv::Image2D& wholeimg = wholeview_v[plane];
    for ( auto& roi : splitroi_v ) {
      const larcv::ImageMeta& bbox = roi.BB(plane);
      std::cout << "crop from the whole image" << std::endl;
      try {
	larcv::Image2D crop = wholeimg.crop( bbox );
	splitimg_v.emplace_back( std::move(crop) );
      }
      catch( std::exception& e ) {
	throw cet::exception("DLInterface") << "error in cropping: " << e.what() << std::endl;
      }
      ncrops++;
    }
  }
  std::cout << "cropped the regions: total " << ncrops << std::endl;

  // done
  return (int)ncrops;
}

/**
 * 
 * serialize image into binary-json message + strings describing image meta
 *
 * @param[in] nplanes number of input plane images
 * @param[in] img_v individual (cropped/split) images to serialize
 * @param[inout] msg_img_v binary json message for each image
 * @param[inout] msg_meta_v string describing meta
 * @param[inout] msg_name_v string describing name
 * 
 */
size_t DLInterface::serializeImages( const size_t nplanes,
				     const std::vector<larcv::Image2D>& img_v,   
				     std::vector< std::vector<unsigned char> >& msg_img_v,
				     std::vector<std::string>& msg_meta_v,
				     std::vector<std::string>& msg_name_v ) {
  
  size_t nimgs = img_v.size();
  size_t nsets = nimgs/nplanes;
  msg_img_v.resize(  nimgs );
  msg_meta_v.resize( nimgs );
  msg_name_v.resize( nimgs );

  size_t img_msg_totsize = 0.;
  
  for (size_t iset=0; iset<nsets; iset++ ) {
    for (size_t p=0; p<nplanes; p++) {
      size_t iimg = nplanes*iset + p;
      const larcv::Image2D& img = img_v[ iimg ];

      char zname[100];
      sprintf( zname, "croicropped_p%d_iset%d_iimg%d", (int)p, (int)iset, (int)iimg );
      
      std::string msg_name = zname;
      std::string msg_meta = img.meta().dump();

      msg_img_v[ iimg ]  = larcv::json::as_bson( img );
      msg_meta_v[ iimg ] = img.meta().dump();
      msg_name_v[ iimg ] = zname;
      
      img_msg_totsize += msg_img_v[iimg].size();
    }
  }
  
  std::cout << "Created " << msg_img_v.size() << " messages. Total image message size: " << img_msg_totsize << " (chars)" << std::endl;
  return img_msg_totsize;
}

/**
 * 
 * run ssnet via GPU server
 *
 * 
 */
int DLInterface::runSSNetServer( const std::vector<larcv::Image2D>& wholeview_v, 
				 std::vector<larcv::Image2D>& splitimg_v, 
				 std::vector<larcv::ROI>& splitroi_v, 
				 std::vector<larcv::Image2D>& showerout_v,
				 std::vector<larcv::Image2D>& trackout_v ) {
  int status = 0;

  // talk to the socket
  // for ( size_t imsg=0; imsg<msg_img_v.size(); imsg++ ) {

  //   // first the name
  //   std::cout << "sending name[" << imsg << "] ... ";
  //   _socket->send( msg_name_v[imsg].c_str(), msg_name_v[imsg].length(), ZMQ_SNDMORE );
  //   std::cout << " done" << std::endl;

  //   // meta
  //   std::cout << "sending meta[" << imsg << "] ... ";
  //   _socket->send( msg_meta_v[imsg].c_str(), msg_meta_v[imsg].length(), ZMQ_SNDMORE );
  //   std::cout << " done" << std::endl;

  //   // image
  //   std::cout << "sending image[" << imsg << "] ... size=" << msg_img_v[imsg].size() << " ... ";
  //   if (imsg+1!=msg_img_v.size()) {
  //     _socket->send( msg_img_v[imsg].data(), msg_img_v[imsg].size(), ZMQ_SNDMORE );
  //     std::cout << " done" << std::endl;
  //   }
  //   else {
  //     _socket->send( msg_img_v[imsg].data(), msg_img_v[imsg].size() );
  //     std::cout << "last message sent" << std::endl;
  //   }
    
  // }
  // std::cout << "Event messages Sent!" << std::endl;
  
  // zmq::message_t reply;
  // _socket->recv (&reply);
  // std::cout << "Received: " << reply.data() << std::endl;
  
  return status;
}


/**
 * 
 * run networking using pytorch CPU C++ interface
 *
 * 
 */
int DLInterface::runPyTorchCPU( const std::vector<larcv::Image2D>& wholeview_v, 
				std::vector<larcv::Image2D>& splitimg_v, 
				std::vector<larcv::ROI>& splitroi_v, 
				std::vector<larcv::Image2D>& showerout_v,
				std::vector<larcv::Image2D>& trackout_v) {

#ifdef HAS_TORCH

  size_t nimgs   = splitimg_v.size();


  //run the net!
  size_t iimg = 0;
  showerout_v.clear();
  showerout_v.reserve( nimgs+10 );
  trackout_v.clear();
  trackout_v.reserve( nimgs+10 );
  for ( auto& img : splitimg_v ) {
    std::cout << "img[" << iimg << "] converting to aten::tensor" << std::endl;
    std::vector<torch::jit::IValue> input;
    input.push_back( larcv::torchutils::as_tensor( img ).reshape( {1,1,(int)img.meta().cols(),(int)img.meta().rows()} ) );

    at::Tensor output;
    try {
      output = _module->forward(input).toTensor();
      std::cout << "img[" << iimg << "] network produced ssnet img dim=";
      for ( int i=0; i<output.dim(); i++ )
	std::cout << output.size(i) << " ";
      std::cout << std::endl;
    }
    catch (std::exception& e) {
      throw cet::exception("DLInterface") << "module error while running img[" << iimg << "]: " << e.what() << std::endl;
    }

    // output is {1,3,H,W} with values being log(softmax)
    at::Tensor shower_slice = output.slice(1, 0, 1).exp(); // dim, start, end
    at::Tensor track_slice  = output.slice(1, 1, 2).exp();
    std::cout << "img[" << iimg << "] slice dim=";
    for ( int i=0; i<shower_slice.dim(); i++ )
      std::cout << shower_slice.size(i) << " ";
    std::cout << std::endl;


    // as img2d
    std::cout << "img[" << iimg << "] converting out back to image2d" << std::endl;
    try {
      larcv::Image2D shrout = larcv::torchutils::image2d_fromtorch( shower_slice, 
								    splitimg_v[iimg].meta() );
      larcv::Image2D trkout = larcv::torchutils::image2d_fromtorch( track_slice,  
								    splitimg_v[iimg].meta() );
      std::cout << "img[" << iimg << "] conversion complete. store." << std::endl;
      showerout_v.emplace_back( std::move(shrout) );
      trackout_v.emplace_back(  std::move(trkout) );
    }
    catch (std::exception& e ) {
      throw cet::exception("DLInterface") << "error in converting tensor->image2d: " << e.what() << std::endl;
    }
    
    iimg++;
  }
  
#endif // HAS_TORCH
  
  return 0;
}


/**
 * 
 * merge ssnet output into a wholeview image
 *
 * 
 */
void DLInterface::mergeSSNetOutput( const std::vector<larcv::Image2D>& wholeview_v, 
				    const std::vector<larcv::ROI>& splitroi_v,
				    const std::vector<larcv::Image2D>& showerout_v,
				    const std::vector<larcv::Image2D>& trackout_v,
				    std::vector<larcv::Image2D>& showermerged_v,
				    std::vector<larcv::Image2D>& trackmerged_v) {
				    
  
  // make output images
  showermerged_v.clear();
  trackmerged_v.clear();
  for ( auto const& img : wholeview_v ) {
    larcv::Image2D shrout( img.meta() );
    shrout.paint(0);
    showermerged_v.emplace_back( std::move(shrout) );

    larcv::Image2D trkout( img.meta() );
    trkout.paint(0);
    trackmerged_v.emplace_back( std::move(trkout) );
  }

  // loop through ssnetoutput, put values into image
  for ( size_t iimg=0; iimg<showerout_v.size(); iimg++ ) {
    auto const& shrout = showerout_v[iimg];
    auto& shrmerge     = showermerged_v.at( shrout.meta().plane() );
    shrmerge.overlay( shrout, larcv::Image2D::kOverWrite );

    auto const& trkout = trackout_v[iimg];
    auto& trkmerge     = showermerged_v.at( trkout.meta().plane() );
    trkmerge.overlay( trkout, larcv::Image2D::kOverWrite );
  }
}

/**
 * 
 * overridden method: setup class before job is run
 *
 * 
 */
void DLInterface::beginJob()
{
  _supera.initialize();

  switch( _interface ) {
  case kServer:
  case kDummyServer:
    openServerSocket();
    break;
  case kPyTorchCPU:
#ifdef HAS_TORCH
    try {
      _module = torch::jit::load( _pytorch_net_script );
    }
    catch (std::exception& e) {
      throw cet::exception("DLInterface") << "Could not load model from " << _pytorch_net_script 
					  << ": "
					  << e.what()
					  << std::endl;
    }
    if ( _module==nullptr )
      throw cet::exception("DLInterface") << "model loaded as NULL " << _pytorch_net_script << std::endl;
    std::cout << "Network Loaded" << std::endl;
#endif
    break;
  case kTensorFlowCPU:
    break;
  default:
    _context = nullptr;
    _socket  = nullptr;
    break;
  }

}

/**
 * 
 * overridden method: setup class after all events are run
 *
 * 
 */
void DLInterface::endJob()
{
  std::cout << "DLInterface::endJob -- start" << std::endl;
  std::cout << "DLinterface::endJob -- finalize IOmanager" << std::endl;
  _supera.finalize();
  std::cout << "DLInterface::endJob -- finished" << std::endl;
}

/**
 * 
 * load pytorch network from torch script file
 *
 * 
 */
void DLInterface::loadNetwork_PyTorchCPU() {
#ifdef HAS_TORCH
  std::cout << "Loading network from " << _pytorch_net_script << " .... " << std::endl;
  
  std::shared_ptr<torch::jit::script::Module> module = nullptr;
  try {
    _module = torch::jit::load( _pytorch_net_script );
  }
  catch (...) {
    throw cet::exception("DLInterface") << "Could not load model from " << _pytorch_net_script << std::endl;
  }
  if ( _module==nullptr )
    throw cet::exception("DLInterface") << "model loaded as NULL " << _pytorch_net_script << std::endl;
  std::cout << "Network Loaded" << std::endl;
#endif
}


/**
 * 
 * open the server socket
 *
 * 
 */
void DLInterface::openServerSocket() {

  std::cout << "DLInterface: open server socket" << std::endl;
  
  // open the zmq socket
  _context = new zmq::context_t(1);
  _socket  = new zmq::socket_t( *_context, ZMQ_REQ );
  char identity[10];
  sprintf(identity,"larbys00"); //to-do: replace with pid or some unique identifier
  _socket->setsockopt( ZMQ_IDENTITY, identity, 8 );

  if ( _interface==kDummyServer ) 
    _socket->connect("tcp://localhost:5555");
  else
    throw cet::exception("DLInterface") << "connection for gpu-server not defined yet" << std::endl;

}

/**
 * 
 * close the server socket
 *
 * 
 */
void DLInterface::closeServerSocket() {

  std::cout << "DLInterface: close server socket" << std::endl;

  // close zmq socket
  if ( _socket )
    delete _socket;
  if ( _context )
    delete _context;

  _socket  = nullptr;
  _context = nullptr;
}

/**
 * 
 * for debug: send a dummy message to the dummy server
 *
 * 
 */
void DLInterface::sendDummyMessage() {

  std::cout << "DLInterface: close server socket" << std::endl;

  //  Do 10 requests, waiting each time for a response
  for (int request_nbr = 0; request_nbr != 10; request_nbr++) {
    zmq::message_t request (5);
    memcpy (request.data (), "Hello", 5);
    std::cout << "Sending Hello " << request_nbr << "…" << std::endl;
    _socket->send (request);
    
    //  Get the reply.
    zmq::message_t reply;
    _socket->recv (&reply);
    std::cout << "Received World " << request_nbr << std::endl;
  }
  
  std::cout << "end of sendDummyMessage()" << std::endl;
}


DEFINE_ART_MODULE(DLInterface)
