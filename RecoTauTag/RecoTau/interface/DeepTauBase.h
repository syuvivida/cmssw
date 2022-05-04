#ifndef RecoTauTag_RecoTau_DeepTauBase_h
#define RecoTauTag_RecoTau_DeepTauBase_h

/*
 * \class DeepTauBase
 *
 * Definition of the base class for tau identification using Deep NN.
 *
 * \author Konstantin Androsov, INFN Pisa
 * \author Maria Rosaria Di Domenico, University of Siena & INFN Pisa
 */

#include <Math/VectorUtil.h>
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "PhysicsTools/TensorFlow/interface/TensorFlow.h"
#include "tensorflow/core/util/memmapped_file_system.h"
#include "DataFormats/PatCandidates/interface/Electron.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/Tau.h"
#include "DataFormats/TauReco/interface/TauDiscriminatorContainer.h"
#include "DataFormats/TauReco/interface/PFTauDiscriminator.h"
#include "DataFormats/PatCandidates/interface/PATTauDiscriminator.h"
#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "RecoTauTag/RecoTau/interface/PFRecoTauClusterVariables.h"
#include "RecoTauTag/RecoTau/interface/DeepTauScaling.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/Common/interface/View.h"
#include "DataFormats/Common/interface/RefToBase.h"
#include "DataFormats/Provenance/interface/ProductProvenance.h"
#include "DataFormats/Provenance/interface/ProcessHistoryID.h"
#include "FWCore/Common/interface/Provenance.h"
#include <TF1.h>
#include <map>

namespace deep_tau {

  class TauWPThreshold {
  public:
    explicit TauWPThreshold(const std::string& cut_str);
    double operator()(const reco::BaseTau& tau, bool isPFTau) const;

  private:
    std::unique_ptr<TF1> fn_;
    double value_;
  };

  class DeepTauCache {
  public:
    using GraphPtr = std::shared_ptr<tensorflow::GraphDef>;

    DeepTauCache(const std::map<std::string, std::string>& graph_names, bool mem_mapped);
    ~DeepTauCache();

    // A Session allows concurrent calls to Run(), though a Session must
    // be created / extended by a single thread.
    tensorflow::Session& getSession(const std::string& name = "") const { return *sessions_.at(name); }
    const tensorflow::GraphDef& getGraph(const std::string& name = "") const { return *graphs_.at(name); }

  private:
    std::map<std::string, GraphPtr> graphs_;
    std::map<std::string, tensorflow::Session*> sessions_;
    std::map<std::string, std::unique_ptr<tensorflow::MemmappedEnv>> memmappedEnv_;
  };

  class DeepTauBase : public edm::stream::EDProducer<edm::GlobalCache<DeepTauCache>> {
  public:
    using TauDiscriminator = reco::TauDiscriminatorContainer;
    using TauCollection = edm::View<reco::BaseTau>;
    using CandidateCollection = edm::View<reco::Candidate>;
    using TauRef = edm::Ref<TauCollection>;
    using TauRefProd = edm::RefProd<TauCollection>;
    using ElectronCollection = pat::ElectronCollection;
    using MuonCollection = pat::MuonCollection;
    using LorentzVectorXYZ = ROOT::Math::LorentzVector<ROOT::Math::PxPyPzE4D<double>>;
    using Cutter = TauWPThreshold;
    using CutterPtr = std::unique_ptr<Cutter>;
    using WPList = std::vector<CutterPtr>;

    struct Output {
      std::vector<size_t> num_, den_;

      Output(const std::vector<size_t>& num, const std::vector<size_t>& den) : num_(num), den_(den) {}

      std::unique_ptr<TauDiscriminator> get_value(const edm::Handle<TauCollection>& taus,
                                                  const tensorflow::Tensor& pred,
                                                  const WPList* working_points,
                                                  bool is_online) const;
    };

    using OutputCollection = std::map<std::string, Output>;

    DeepTauBase(const edm::ParameterSet& cfg, const OutputCollection& outputs, const DeepTauCache* cache);
    ~DeepTauBase() override {}

    void produce(edm::Event& event, const edm::EventSetup& es) override;

    static std::unique_ptr<DeepTauCache> initializeGlobalCache(const edm::ParameterSet& cfg);
    static void globalEndJob(const DeepTauCache* cache) {}

    template <typename ConsumeType>
    struct TauDiscInfo {
      edm::InputTag label;
      edm::Handle<ConsumeType> handle;
      edm::EDGetTokenT<ConsumeType> disc_token;
      double cut;
      void fill(const edm::Event& evt) { evt.getByToken(disc_token, handle); }
    };

    // select boolean operation on prediscriminants (and = 0x01, or = 0x00)
    uint8_t andPrediscriminants_;
    std::vector<TauDiscInfo<pat::PATTauDiscriminator>> patPrediscriminants_;
    std::vector<TauDiscInfo<reco::PFTauDiscriminator>> recoPrediscriminants_;

    enum BasicDiscriminator {
      ChargedIsoPtSum,
      NeutralIsoPtSum,
      NeutralIsoPtSumWeight,
      FootprintCorrection,
      PhotonPtSumOutsideSignalCone,
      PUcorrPtSum
    };

  private:
    virtual tensorflow::Tensor getPredictions(edm::Event& event, edm::Handle<TauCollection> taus) = 0;
    virtual void createOutputs(edm::Event& event, const tensorflow::Tensor& pred, edm::Handle<TauCollection> taus);

  protected:
    edm::EDGetTokenT<TauCollection> tausToken_;
    edm::EDGetTokenT<CandidateCollection> pfcandToken_;
    edm::EDGetTokenT<reco::VertexCollection> vtxToken_;
    std::map<std::string, WPList> workingPoints_;
    const bool is_online_;
    OutputCollection outputs_;
    const DeepTauCache* cache_;

    static const std::map<BasicDiscriminator, std::string> stringFromDiscriminator_;
    static const std::vector<BasicDiscriminator> requiredBasicDiscriminators_;
    static const std::vector<BasicDiscriminator> requiredBasicDiscriminatorsdR03_;
  };

  namespace Scaling {

    const std::map<std::pair<FeatureT, bool>, ScalingParams> scalingParamsMap_v2p1 = {
        // {std::make_pair(FeatureT::TauFlat, false), {
        //     // mean_
        //     {21.49, 20.0, 0.0, 0., 0.6669,
        //     1., 0, 1, 0, 47.78,
        //     0, 9.029, 57.59, 0, 0,
        //     0, 1.731, 22.38, -0.0241, 0.0675,
        //     0.7973, 0, 0.0018, 2.26, 0,
        //     0.0026, 2.928, 0., 0, 4.717,
        //     -0.0003, -0.0009, -0.0022, 0., 0.,
        //     0., 0.0052, 0., 1.538, 0,
        //     0., 0, 0., 2.95, 0.0,
        //     0, 0.0042},

        //     // std_ 
        //     {9.713, 980.0, 2.3, 3.141592653589793, 0.6553,
        //     4.2, 1, 2, 2, 123.5,
        //     1, 26.42, 155.3, 1, 1,
        //     1, 6.846, 16.34, 0.0074, 0.0128,
        //     3.456, 1, 0.0085, 4.191, 1,
        //     0.0114, 4.466, 0.0190, 1, 11.78,
        //     0.7362, 0.7354, 1.993, 1, 1.,
        //     1, 0.01433, 1., 4.401, 1,
        //     1, 1, 3.141592653589793, 3.927, 1.,
        //     1.0, 0.0323},

        //     // lim_min_
        //     {-5, 0., -1.0, -1., -5,
        //     0, -inf, 0, 0, -5,
        //     -inf, -5, -5, -inf, -inf,
        //     -inf, -5, -5, -5, -5,
        //     -5, -inf, -5, -5, -inf,
        //     -5, -5, -5, -inf, -5,
        //     -5, -5, -5, -inf, 0,
        //     0, -5, 0, -5, -inf,
        //     0, -inf, 0, -5, -1.0,
        //     -inf, -5},

        //     // lim_max_
        //     {5, 1., 1.0, 1., 5,
        //     1, inf, 1, 1, 5,
        //     inf, 5, 5, inf, inf,
        //     inf, 5, 5, 5, 5,
        //     5, inf, 5, 5, inf,
        //     5, 5, 5, inf, 5,
        //     5, 5, 5, inf, 1,
        //     1, 5, 1, 5, inf,
        //     1, inf, 1, 5, -1.0,
        //     inf, 5},
        //     } 
        // }, // end TauFlat

        // {std::make_pair(FeatureT::GridGlobal, false), {
        //     // mean_
        //     {{21.49,21.49},{20.0,20.0},{0.0,0.0},{0,0}},
        //     // std_
        //     {{9.713,9.713},{980.0,980.0},{2.3,2.3},{1,1}},
        //     // lim_min_
        //     {{-5,-5},{0.,0.},{-1.0,-1.0},{-inf,-inf}},
        //     // lim_max_
        //     {{5,5},{1.,1.},{1.0,1.0},{inf,inf}},
        //     }
        // }, // end GridGlobal

        // {FeatureT::PfCand_electron, {
        //     // mean_
        //     {{0,0},{0.304,0.9792},{0.0,0.0},{0.0,0.0},{0,0},
        //      {0,0},{0,0},{0,0},{0,0},{0,0},
        //      {0,0},{0.001,0.001},{0,0},{0.0003,0.0003},{0,0},
        //      {0,0},{0.,0.},{1.634,1.634},{0.001,0.001},{24.56,24.56},
        //      {2.272,2.272},{15.18,15.18}},

        //     // std_
        //     {{1,1},{1.845,0.5383},{0.5,0.1},{0.5,0.1},{7.,7.},
        //      {1,1},{1,1},{1,1},{10.0,10.0},{0.1221,0.1221},
        //      {0.1226,0.1226},{1.024,1.024},{0.3411,0.3411},{0.3385,0.3385},{1.307,1.307},
        //      {1,1},{0.171,0.171},{6.45,6.45},{1.02,1.02},{210.4,210.4},
        //      {8.439,8.439},{3.203,3.203}},

        //     // lim_min_
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{0.,0.},
        //     {-inf,-inf},{-inf,-inf},{-inf,-inf},{0.,0.},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-inf,-inf},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5}},

        //     // lim_max_
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{1.,1.},
        //     {inf,inf},{inf,inf},{inf,inf},{1.,1.},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {inf,inf},{5,5},{5,5},{5,5},{5,5},
        //     {5,5},{5,5}}
        //     }
        // }, // end PfCand_electron

        // {FeatureT::PfCand_gamma, {
        //     // mean
        //     {{0,0},{0.02576,0.6048},{0.0,0.0},{0.0,0.0},{0,0},
        //      {0,0},{0,0},{0,0},{0,0},{0.,0.},
        //      {0.,0.},{0.,0.},{0.,0.},{0.001,0.001},{0.0008,0.0008},
        //      {0.0038,0.0038},{0,0},{0.0004,0.0004},{4.271,4.271},{0.0071,0.0071},
        //      {162.1,162.1},{4.268,4.268},{12.25,12.25}},
            
        //     // std
        //     {{1,1},{0.3833,1.669},{0.5,0.1},{0.5,0.1},{7.,7.},
        //      {3.,3.},{1,1},{1,1},{1,1},{7.,7.},
        //      {0.0067,0.0067},{0.0069,0.0069},{0.0578,0.0578},{0.9565,0.9565},{0.9592,0.9592},
        //      {2.154,2.154},{1,1},{0.882,0.882},{63.78,63.78},{5.285,5.285},
        //      {622.4,622.4},{15.47,15.47},{4.774,4.774}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{0.,0.},
        //      {0.,0.},{-inf,-inf},{-inf,-inf},{-inf,-inf},{0.,0.},
        //      {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //      {-5,-5},{-inf,-inf},{-5,-5},{-5,-5},{-5,-5},
        //      {-5,-5},{-5,-5},{-5,-5}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{1.,1.},
        //      {1.,1.},{inf,inf},{inf,inf},{inf,inf},{1.,1.},
        //      {5,5},{5,5},{5,5},{5,5},{5,5},
        //      {5,5},{inf,inf},{5,5},{5,5},{5,5},
        //      {5,5},{5,5},{5,5}},
            
        //     }
        // }, // end PfCand_gamma

        // {FeatureT::Electron, {
        //     // mean
        //     {{0,0},{0.5111,1.067},{0.0,0.0},{0.0,0.0},{0,0},
        //     {1.729,1.729},{0.1439,0.1439},{1.794,1.794},{1.531,1.531},{1.531,1.531},
        //     {0.7735,0.7735},{0.7735,0.7735},{1.625,1.625},{1.993,1.993},{70.25,70.25},
        //     {2.432,2.432},{2.034,2.034},{6.64,6.64},{4.183,4.183},{0.,0.},
        //     {-0.0001,-0.0001},{-0.0001,-0.0001},{0.0002,0.0002},{0.0001,0.0001},{0.0004,0.0004},
        //     {0,0},{0,0},{0.0008,0.0008},{14.04,14.04},{0.0099,0.0099},
        //     {3.049,3.049},{16.52,16.52},{1.355,1.355},{5.046,5.046},{0,0},
        //     {2.411,2.411},{15.16,15.16}},

        //     // std 
        //     {{1,1},{2.765,1.521},{0.5,0.1},{0.5,0.1},{1,1},
        //     {1.644,1.644},{0.3284,0.3284},{2.079,2.079},{1.424,1.424},{1.424,1.424},
        //     {0.935,0.935},{0.935,0.935},{1.581,1.581},{1.308,1.308},{58.16,58.16},
        //     {15.13,15.13},{13.96,13.96},{36.8,36.8},{20.63,20.63},{0.0363,0.0363},
        //     {0.0512,0.0512},{0.0541,0.0541},{0.0553,0.0553},{0.0523,0.0523},{0.0777,0.0777},
        //     {1,1},{1,1},{0.0052,0.0052},{69.48,69.48},{0.0851,0.0851},
        //     {10.39,10.39},{2.806,2.806},{16.81,16.81},{3.119,3.119},{1,1},
        //     {6.98,6.98},{5.26,5.26}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{-inf,-inf},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-inf,-inf},{-inf,-inf},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-inf,-inf},
        //     {-5,-5},{-5,-5}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{inf,inf},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {inf,inf},{inf,inf},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{inf,inf},
        //     {5,5},{5,5}},
        //     }
        // }, // end Electron

        // {FeatureT::PfCand_muon, {
        //     // mean
        //     {{0,0},{0.0861,0.9509},{0.0,0.0},{0.0,0.0},{0,0},
        //     {0,0},{0,0},{0,0},{0,0},{0.,0.},
        //     {-0.0007,-0.0007},{0.0001,0.0001},{-0.0117,-0.0117},{-0.0001,-0.0001},{0.0004,0.0004},
        //     {-0.0118,-0.0118},{0,0},{-0.0045,-0.0045},{4.575,4.575},{-0.0117,-0.0117},
        //     {80.37,80.37},{0.69,0.69},{17.5,17.5}},

        //     // std
        //     {{1,1},{0.4065,0.4294},{0.5,0.1},{0.5,0.1},{7.,7.},
        //     {3.,3.},{1,1},{1,1},{1,1},{11.,11.},
        //     {0.6869,0.6869},{0.6784,0.6784},{4.097,4.097},{0.8642,0.8642},{0.8561,0.8561},
        //     {4.405,4.405},{1,1},{0.9655,0.9655},{42.36,42.36},{4.097,4.097},
        //     {343.3,343.3},{1.711,1.711},{5.11,5.11}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{0.,0.},
        //     {0.,0.},{-inf,-inf},{-inf,-inf},{-inf,-inf},{0.,0.},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-inf,-inf},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{1.,1.},
        //     {1.,1.},{inf,inf},{inf,inf},{inf,inf},{1.0,1.0},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {5,5},{inf,inf},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5}},

        //     }
        // }, // end PfCand_muon

        // {FeatureT::Muon, {
        //     // mean
        //     {{0,0},{0.2678,0.7966},{0.0,0.0},{0.0,0.0},{0.0019,0.0019},
        //     {8.98,8.98},{0,0},{21.52,21.52},{21.84,21.84},{0,0},
        //     {0,0},{0,0},{0.2273,0.2273},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.}},

        //     // std
        //     {{1,1},{3.592,3.402},{0.5,0.1},{0.5,0.1},{1.039,1.039},
        //     {71.17,71.17},{1,1},{265.8,265.8},{10.59,10.59},{1,1},
        //     {1,1},{1,1},{0.4865,0.4865},{2.0,2.0},{2.0,2.0},
        //     {2.0,2.0},{2.0,2.0},{6.0,6.0},{2.0,2.0},{2.0,2.0},
        //     {2.0,2.0},{7.,7.},{6.0,6.0},{4.0,4.0},{4.0,4.0},
        //     {12.0,12.0},{12.0,12.0},{12.0,12.0},{8.0,8.0},{24.0,24.0},
        //     {12.0,12.0},{12.0,12.0},{12.0,12.0},{4.0,4.0},{4.0,4.0},
        //     {2.0,2.0},{2.0,2.0}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{-5,-5},
        //     {-5,-5},{-inf,-inf},{-5,-5},{-5,-5},{-inf,-inf},
        //     {-inf,-inf},{-inf,-inf},{-5,-5},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.},{0.,0.},{0.,0.},{0.,0.},
        //     {0.,0.},{0.,0.}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{5,5},
        //     {5,5},{inf,inf},{5,5},{5,5},{inf,inf},
        //     {inf,inf},{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},
        //     {1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},
        //     {1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},
        //     {1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},
        //     {1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},{1.0,1.0},
        //     {1.0,1.0},{1.0,1.0}},

        //     }
        // }, // end Muon

        // {FeatureT::PfCand_chHad, {
        //     // mean
        //     {{0,0},{0.0194,0.2564},{0.0,0.0},{0.0,0.0},{0,0},
        //     {0,0},{0,0},{0,0},{0,0},{0,0},
        //     {0,0},{0.,0.},{0.0005,0.0005},{-0.0008,-0.0008},{-0.0201,-0.0201},
        //     {-0.0014,-0.0014},{0.0022,0.0022},{-0.0138,-0.0138},{0,0},{-0.012,-0.012},
        //     {6.417,6.417},{-0.0246,-0.0246},{301.3,301.3},{0.7876,0.7876},{13.92,13.92},
        //     {0,0},{0.,0.}},

        //     // std 
        //     {{1,1},{0.1865,0.8607},{0.5,0.1},{0.5,0.1},{1,1},
        //     {7,7},{3,3},{1,1},{1,1},{1,1},
        //     {1,1},{12.0,12.0},{1.735,1.735},{1.752,1.752},{8.333,8.333},
        //     {1.93,1.93},{1.948,1.948},{8.622,8.622},{1,1},{2.386,2.386},
        //     {36.28,36.28},{7.618,7.618},{491.1,491.1},{3.694,3.694},{6.581,6.581},
        //     {1,1},{2.6,2.6}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{-inf,-inf},
        //     {0.,0.},{0.,0.},{-inf,-inf},{-inf,-inf},{-inf,-inf},
        //     {-inf,-inf},{0.,0.},{-5,-5},{-5,-5},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-inf,-inf},{-5,-5},
        //     {-5,-5},{-5,-5},{-5,-5},{-5,-5},{-5,-5},
        //     {-inf,-inf},{0.,0.}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{inf,inf},
        //     {1.,1.},{1.,1.},{inf,inf},{inf,inf},{inf,inf},
        //     {inf,inf},{1.0,1.0},{5,5},{5,5},{5,5},
        //     {5,5},{5,5},{5,5},{inf,inf},{5,5},
        //     {5,5},{5,5},{5,5},{5,5},{5,5},
        //     {inf,inf},{1.0,1.0}},
        //     }
        // }, // end PfCand_chHad

        // {FeatureT::PfCand_nHad, {
        //     // mean 
        //     {{0,0},{0.0502,0.3163},{0.0,0.0},{0.0,0.0},{0,0},
        //     {0,0},{0,0}},

        //     // std 
        //     {{1,1},{0.4266,0.2769},{0.5,0.1},{0.5,0.1},{1,1},
        //     {1,1},{1,1}},

        //     // lim_min
        //     {{-inf,-inf},{-5,-5},{-1.0,-1.0},{-1.0,-1.0},{-inf,-inf},
        //     {-inf,-inf},{-inf,-inf}},

        //     // lim_max
        //     {{inf,inf},{5,5},{1.0,1.0},{1.0,1.0},{inf,inf},
        //     {inf,inf},{inf,inf}},

        //     }
        // }, // end PfCand_nHad

    }; // end scalingParamsMap_v2p1

    const std::map<std::pair<FeatureT, bool>, ScalingParams> scalingParamsMap_v2p5 = {
        {std::make_pair(FeatureT::TauFlat, false), {
            // mean_
            {25.0, 510.0, 0.0, 0.5762, 1.967,
            0, 1, 0, 14.32, 0,
            2.213, 11.36, 0, 0, 0,
            1.202, 22.17, 0, 0.002281, 2.392,
            0, 0.00318, 2.991, 3.212e-05, 0,
            16.75, -0.0008515, -0.0001629, -0.0007875, -5.564,
            0.5, 0.5, 0.007766, 0.5, 1.672,
            0, 0.5, 0, 1.5707963267948966, 2.256,
            0.0, 0, 0.0002029},

            // std_ 
            {25.0, 490.0, 2.3, 0.5293, 1.133,
            1, 1, 1, 44.8, 1,
            6.783, 48.09, 1, 1, 1,
            3.739, 13.68, 1, 0.009705, 4.187,
            1, 0.01452, 4.527, 0.4518, 1,
            191.7, 0.4016, 0.4041, 1.157, 8.72,
            0.5, 0.5, 0.01834, 0.5, 5.058,
            1, 0.5, 1, 1.5707963267948966, 2.943,
            1.0, 1, 0.03612},

            // lim_min_
            {-1.0, -1.0, -1.0, -5, -5,
            -inf, -inf, -inf, -5, -inf,
            -5, -5, -inf, -inf, -inf,
            -5, -5, -inf, -5, -5,
            -inf, -5, -5, -5, -inf,
            -5, -5, -5, -5, -5,
            -1.0, -1.0, -5, -1.0, -5,
            -inf, -1.0, -inf, -1.0, -5,
            -1.0, -inf, -5},

            // lim_max_
            {1.0, 1.0, 1.0, 5, 5,
            inf, inf, inf, 5, inf,
            5, 5, inf, inf, inf,
            5, 5, inf, 5, 5,
            inf, 5, 5, 5, inf,
            5, 5, 5, 5, 5,
            1.0, 1.0, 5, 1.0, 5,
            inf, 1.0, inf, 1.0, 5,
            1.0, inf, 5},
            } 
        }, // end TauFlat

        {std::make_pair(FeatureT::GridGlobal, false), {
            // mean_
            {25.0,510.0,0.0,0,},
            // std_
            {25.0,490.0,2.3,1,},
            // lim_min_
            {-1.0,-1.0,-1.0,-inf},
            // lim_max_
            {1.0,1.0,1.0,inf},
          }
        }, // end GridGlobal

        {std::make_pair(FeatureT::PfCand_electron, false), {
            // mean_
            {0,0.3457,0.0,0.0,0,
             0,0,0,5.0,-0.0008022,
             -2.653e-05,0.00382,0.002371,0.0003833,0.0004431,
             0,0.000397,3.409,0.003507,169.6,
             4.561,12.6},

            // std_
            {1,1.164,0.5,0.5,1,
             1,1,1,5.0,0.4081,
             0.4056,3.329,0.6623,0.6648,3.548,
             1,0.5572,16.07,3.3,486.1,
             14.8,3.729},

            // lim_min_
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-1.0,-5,
            -5,-5,-5,-5,-5,
            -inf,-5,-5,-5,-5,
            -5,-5},

            // lim_max_
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,1.0,5,
            5,5,5,5,5,
            inf,5,5,5,5,
            5,5}
            }
        }, // end PfCand_electron, is_inner=false

        {std::make_pair(FeatureT::PfCand_electron, true), {
            // mean_
            {0,0.9558,0.0,0.0,0,
             0,0,0,5.0,-2.888e-06,
             7.215e-06,0.0002156,0.0002385,6.221e-05,0.0003546,
             0,3.333e-05,1.412,0.0002181,21.72,
             2.387,14.73},

            // std_
            {1,0.2323,0.1,0.1,1,
             1,1,1,5.0,0.03703,
             0.03682,0.5552,0.1855,0.1867,0.749,
             1,0.05183,3.111,0.5551,230.5,
             8.818,3.125},

            // lim_min_
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-1.0,-5,
            -5,-5,-5,-5,-5,
            -inf,-5,-5,-5,-5,
            -5,-5},

            // lim_max_
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,1.0,5,
            5,5,5,5,5,
            inf,5,5,5,5,
            5,5}
            }
        }, // end PfCand_electron, is_inner=true
    
        {std::make_pair(FeatureT::PfCand_gamma, false), {
            // mean
            {0,0.02024,0.0,0.0,0,
             0,0,0,0,3.5,
             2.364e-08,-1.355e-07,5.947e-07,0.001155,-3.88e-05,
             0.001081,0,0.003532,4.09,0.02207,
             175.0,4.798,12.18},
            
            // std
            {1,0.1801,0.5,0.5,1,
             1,1,1,1,3.5,
             0.003674,0.00371,0.02345,0.4628,0.4667,
             1.057,1,1.006,11.45,4.517,
             546.1,16.85,4.741},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
             -inf,-inf,-inf,-inf,-1.0,
             -5,-5,-5,-5,-5,
             -5,-inf,-5,-5,-5,
             -5,-5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
             inf,inf,inf,inf,1.0,
             5,5,5,5,5,
             5,inf,5,5,5,
             5,5,5},

            }
        }, // end PfCand_gamma, is_inner=false

        {std::make_pair(FeatureT::PfCand_gamma, true), {
            // mean
            {0,0.2681,0.0,0.0,0,
             0,0,0,0,3.5,
             -6.701e-06,4.799e-06,3.08e-05,0.0009319,-0.0001133,
             0.0007838,0,-0.0003009,3.826,0.01115,
             114.2,4.218,12.27},
            
            // std
            {1,0.5467,0.1,0.1,1,
             1,1,1,1,3.5,
             0.02348,0.02357,0.2203,0.4899,0.4941,
             1.284,1,0.633,20.83,4.191,
             439.3,15.84,4.562},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
             -inf,-inf,-inf,-inf,-1.0,
             -5,-5,-5,-5,-5,
             -5,-inf,-5,-5,-5,
             -5,-5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
             inf,inf,inf,inf,1.0,
             5,5,5,5,5,
             5,inf,5,5,5,
             5,5,5},

            }
        }, // end PfCand_gamma, is_inner=true

        {std::make_pair(FeatureT::Electron, false), {
            // mean
            {0,0.3827,0.0,0.0,0,
            1.37,0.3215,1.793,1.093,1.093,
            1.013,1.013,1.063,1.445,13.07,
            3.797,2.624,5.68,2.231,-0.0001921,
            -0.0009969,-0.0008593,-0.0008999,-0.001147,-0.001182,
            0,0,0.001218,31.5,0.05644,
            6.344,14.65,1.917,6.866,0,
            1.862,12.15},

            // std 
            {1,1.272,0.5,0.5,1,
            8.381,0.5275,2.419,82.69,82.69,
            673.8,673.8,5.614,2.021,27.8,
            21.65,19.0,41.93,21.58,0.1324,
            0.1474,0.1548,0.1514,0.1452,0.1966,
            1,1,0.00775,82.72,0.2343,
            292.7,3.103,229.2,5.051,1,
            5.64,5.557},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -inf,-inf,-5,-5,-5,
            -5,-5,-5,-5,-inf,
            -5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
            5,5,5,5,5,
            5,5,5,5,5,
            5,5,5,5,5,
            5,5,5,5,5,
            inf,inf,5,5,5,
            5,5,5,5,inf,
            5,5},
            }
        }, // end Electron, is_inner=false

        {std::make_pair(FeatureT::Electron, true), {
            // mean
            {0,0.9372,0.0,0.0,0,
            1.654,0.1878,2.055,2.593,2.593,
            1.006,1.006,1.749,2.0,59.55,
            1.748,1.404,5.054,3.078,-4.413e-06,
            -1.477e-05,9.209e-07,0.0001262,8.781e-05,0.0003861,
            0,0,0.000632,15.88,0.005635,
            3.163,16.15,1.669,5.276,0,
            2.813,14.46},

            // std 
            {1,0.4817,0.1,0.1,1,
            1.104,0.3595,2.141,1183.0,1183.0,
            233.5,233.5,88.75,1.278,44.9,
            2.591,2.199,14.8,10.23,0.0119,
            0.02151,0.02331,0.03042,0.03347,0.05816,
            1,1,0.004139,50.36,0.05148,
            15.01,2.752,431.6,2.463,1,
            8.186,5.149},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -5,-5,-5,-5,-5,
            -inf,-inf,-5,-5,-5,
            -5,-5,-5,-5,-inf,
            -5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
            5,5,5,5,5,
            5,5,5,5,5,
            5,5,5,5,5,
            5,5,5,5,5,
            inf,inf,5,5,5,
            5,5,5,5,inf,
            5,5},
            }
        }, // end Electron, is_inner=true

        {std::make_pair(FeatureT::PfCand_muon, false), {
            // mean
            {0,0.142,0.0,0.0,0,
            0,0,0,0,5.5,
            -9.307e-05,-0.0008956,-0.01717,0.001419,-0.0001845,
            -0.01638,0,-0.008642,10.87,-0.01718,
            296.6,0.7838,17.99},

            // std
            {1,0.618,0.5,0.5,1,
            1,1,1,1,5.5,
            1.123,1.108,6.913,1.229,1.216,
            7.147,1,1.578,58.34,6.915,
            515.9,1,2.933,6.317},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-inf,-1.0,
            -5,-5,-5,-5,-5,
            -5,-inf,-5,-5,-5,
            -5,-5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,inf,1.0,
            5,5,5,5,5,
            5,inf,5,5,5,
            5,5,5},

            }
        }, // end PfCand_muon, is_inner=false

        {std::make_pair(FeatureT::PfCand_muon, true), {
            // mean
            {0,0.9561,0.0,0.0,0,
            0,0,0,0,5.5,
            -9.493e-06,2.109e-06,-0.005042,0.0001233,-1.605e-06,
            -0.004842,0,-2.842e-05,1.391,-0.005043,
            10.48,0.5868,17.11},

            // std
            {1,0.1959,0.1,0.1,1,
            1,1,1,1,5.5,
            0.0752,0.07712,0.8103,0.2137,0.2138,
            0.9617,1,0.1077,7.796,0.8103,
            155.9,1.003,4.29},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-inf,-1.0,
            -5,-5,-5,-5,-5,
            -5,-inf,-5,-5,-5,
            -5,-5,-5},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,inf,1.0,
            5,5,5,5,5,
            5,inf,5,5,5,
            5,5,5},

            }
        }, // end PfCand_muon, is_inner=true

        {std::make_pair(FeatureT::Muon, false), {
            // mean
            {0,0.3645,0.0,0.0,0.00344,
            17.54,0,24.78,17.92,0,
            0,0,0.3221,1.0,1.0,
            1.0,1.0,3.0,1.0,1.0,
            1.0,3.5,3.0,2.0,2.0,
            6.0,6.0,6.0,4.0,12.0,
            6.0,6.0,6.0,2.0,2.0,
            1.0,1.0},

            // std
            {1,85.0,0.5,0.5,1.557,
            97.45,1,2022.0,6.573,1,
            1,1,0.6166,1.0,1.0,
            1.0,1.0,3.0,1.0,1.0,
            1.0,3.5,3.0,2.0,2.0,
            6.0,6.0,6.0,4.0,12.0,
            6.0,6.0,6.0,2.0,2.0,
            1.0,1.0},

            // lim_min
            {-inf,-5,-1.0,-1.0,-5,
            -5,-inf,-5,-5,-inf,
            -inf,-inf,-5,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0},

            // lim_max
            {inf,5,1.0,1.0,5,
            5,inf,5,5,inf,
            inf,inf,5,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0},

            }
        }, // end Muon, is_inner=false

        {std::make_pair(FeatureT::Muon, true), {
            // mean
            {0,1.033,0.0,0.0,0.001217,
            5.403,0,7.2,18.58,0,
            0,0,0.09762,1.0,1.0,
            1.0,1.0,3.0,1.0,1.0,
            1.0,3.5,3.0,2.0,2.0,
            6.0,6.0,6.0,4.0,12.0,
            6.0,6.0,6.0,2.0,2.0,
            1.0,1.0},

            // std
            {1,65.51,0.1,0.1,0.2033,
            36.07,1,263.3,5.019,1,
            1,1,0.3956,1.0,1.0,
            1.0,1.0,3.0,1.0,1.0,
            1.0,3.5,3.0,2.0,2.0,
            6.0,6.0,6.0,4.0,12.0,
            6.0,6.0,6.0,2.0,2.0,
            1.0,1.0},

            // lim_min
            {-inf,-5,-1.0,-1.0,-5,
            -5,-inf,-5,-5,-inf,
            -inf,-inf,-5,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0,-1.0,-1.0,-1.0,
            -1.0,-1.0},

            // lim_max
            {inf,5,1.0,1.0,5,
            5,inf,5,5,inf,
            inf,inf,5,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0,1.0,1.0,1.0,
            1.0,1.0},

            }
        }, // end Muon, is_inner=true

        {std::make_pair(FeatureT::PfCand_chHad, false), {
            // mean
            {0,0.02191,0.0,0.0,0,
            0,0,0,0,0,
            0,6.0,0.00106,-0.001523,-0.008181,
            0.004498,4.287e-06,-0.007022,0,-0.01495,
            6.04,-0.01381,323.5,0.753,13.6,
            0,1.3},

            // std 
            {1,0.08964,0.5,0.5,1,
            1,1,1,1,1,
            1,6.0,1.732,1.741,8.372,
            1.783,1.792,8.447,1,2.481,
            33.16,8.026,443.9,3.439,6.624,
            1,1.3},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-inf,-inf,
            -inf,-1.0,-5,-5,-5,
            -5,-5,-5,-inf,-5,
            -5,-5,-5,-5,-5,
            -inf,-1.0},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,inf,inf,
            inf,1.0,5,5,5,
            5,5,5,inf,5,
            5,5,5,5,5,
            inf,1.0},
            }
        }, // end PfCand_chHad, is_inner=false

        {std::make_pair(FeatureT::PfCand_chHad, true), {
            // mean
            {0,0.2482,0.0,0.0,0,
            0,0,0,0,0,
            0,6.0,0.0003524,-0.0003693,-0.002133,
            0.003532,0.000612,-0.0003197,0,-0.001701,
            4.04,-0.002282,61.12,0.9004,14.73,
            0,1.3},

            // std 
            {1,0.3601,0.1,0.1,1,
            1,1,1,1,1,
            1,6.0,0.8533,0.8569,4.132,
            1.033,1.039,4.436,1,1.001,
            16.62,3.254,244.4,4.37,5.599,
            1,1.3},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf,-inf,-inf,-inf,
            -inf,-1.0,-5,-5,-5,
            -5,-5,-5,-inf,-5,
            -5,-5,-5,-5,-5,
            -inf,-1.0},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf,inf,inf,inf,
            inf,1.0,5,5,5,
            5,5,5,inf,5,
            5,5,5,5,5,
            inf,1.0},
            }
        }, // end PfCand_chHad, is_inner=true

        {std::make_pair(FeatureT::PfCand_nHad, false), {
            // mean 
            {0,0.05398,0.0,0.0,0,
            0,0},

            // std 
            {1,0.2929,0.5,0.5,1,
            1,1},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf},

            }
        }, // end PfCand_nHad, is_inner=false

        {std::make_pair(FeatureT::PfCand_nHad, true), {
            // mean 
            {0,0.2553,0.0,0.0,0,
            0,0},

            // std 
            {1,0.2687,0.1,0.1,1,
            1,1},

            // lim_min
            {-inf,-5,-1.0,-1.0,-inf,
            -inf,-inf},

            // lim_max
            {inf,5,1.0,1.0,inf,
            inf,inf},

            }
        }, // end PfCand_nHad, is_inner=true

    }; // end scalingParamsMap_v2p5

  }; // end Scaling namespace

}  // namespace deep_tau

#endif
