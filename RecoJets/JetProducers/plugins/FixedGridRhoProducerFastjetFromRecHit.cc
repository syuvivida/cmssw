/*

Author: Swagata Mukherjee

Date: November 2021

This producer computes rho from ECAL and HCAL recHits. 
The current plan is to use it in egamma and muon HLT, who currently use 
the other producer called FixedGridRhoProducerFastjet.
At HLT, FixedGridRhoProducerFastjet takes calotowers as input and computes rho.
But calotowers are expected to be phased out sometime in Run3.
So this recHit-based rho producer, FixedGridRhoProducerFastjetFromRecHit, can be used as an alternative.

*/

#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "DataFormats/HcalRecHit/interface/HcalRecHitCollections.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "CondFormats/EcalObjects/interface/EcalPFRecHitThresholds.h"
#include "CondFormats/DataRecord/interface/EcalPFRecHitThresholdsRcd.h"
#include "fastjet/tools/GridMedianBackgroundEstimator.hh"
#include "RecoEgamma/EgammaIsolationAlgos/interface/EgammaHcalIsolation.h"

class FixedGridRhoProducerFastjetFromRecHit : public edm::stream::EDProducer<> {
public:
  explicit FixedGridRhoProducerFastjetFromRecHit(const edm::ParameterSet &iConfig);
  ~FixedGridRhoProducerFastjetFromRecHit() override;
  static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);

private:
  void produce(edm::Event &, const edm::EventSetup &) override;
  void getHitP4(const DetId &detId, float hitE, math::XYZTLorentzVector &hitp4, const CaloGeometry &caloGeometry);
  bool passedHcalNoiseCut(const HBHERecHit &hit);
  bool passedEcalNoiseCut(const EcalRecHit &hit, const EcalPFRecHitThresholds &thresholds);

  fastjet::GridMedianBackgroundEstimator bge_;
  const edm::EDGetTokenT<HBHERecHitCollection> hbheRecHitsTag_;
  const edm::EDGetTokenT<EcalRecHitCollection> ebRecHitsTag_;
  const edm::EDGetTokenT<EcalRecHitCollection> eeRecHitsTag_;

  const EgammaHcalIsolation::arrayHB eThresHB_;
  const EgammaHcalIsolation::arrayHE eThresHE_;

  //Muon HLT currently use ECAL-only rho for ECAL isolation,
  //and HCAL-only rho for HCAL isolation. So, this skipping option is needed.
  bool skipHCAL_;
  bool skipECAL_;

  const edm::ESGetToken<EcalPFRecHitThresholds, EcalPFRecHitThresholdsRcd> ecalPFRecHitThresholdsToken_;
  const edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeometryToken_;
};

FixedGridRhoProducerFastjetFromRecHit::FixedGridRhoProducerFastjetFromRecHit(const edm::ParameterSet &iConfig)
    : bge_(iConfig.getParameter<double>("maxRapidity"), iConfig.getParameter<double>("gridSpacing")),
      hbheRecHitsTag_(consumes(iConfig.getParameter<edm::InputTag>("hbheRecHitsTag"))),
      ebRecHitsTag_(consumes(iConfig.getParameter<edm::InputTag>("ebRecHitsTag"))),
      eeRecHitsTag_(consumes(iConfig.getParameter<edm::InputTag>("eeRecHitsTag"))),
      eThresHB_(iConfig.getParameter<EgammaHcalIsolation::arrayHB>("eThresHB")),
      eThresHE_(iConfig.getParameter<EgammaHcalIsolation::arrayHE>("eThresHE")),
      skipHCAL_(iConfig.getParameter<bool>("skipHCAL")),
      skipECAL_(iConfig.getParameter<bool>("skipECAL")),
      ecalPFRecHitThresholdsToken_{esConsumes()},
      caloGeometryToken_{esConsumes()} {
  if (skipHCAL_ && skipECAL_) {
    throw cms::Exception("FixedGridRhoProducerFastjetFromRecHit")
        << "skipHCAL and skipECAL both can't be True. Please make at least one of them False.";
  }
  produces<double>();
}

void FixedGridRhoProducerFastjetFromRecHit::fillDescriptions(edm::ConfigurationDescriptions &descriptions) {
  edm::ParameterSetDescription desc;
  //We use raw recHits and not the PF recHits, because in Phase1 muon and egamma HLT,
  //PF recHit collection is regional, while the full raw recHit collection is available.
  desc.add<edm::InputTag>("hbheRecHitsTag", edm::InputTag("hltHbhereco"));
  desc.add<edm::InputTag>("ebRecHitsTag", edm::InputTag("hltEcalRecHit", "EcalRecHitsEB"));
  desc.add<edm::InputTag>("eeRecHitsTag", edm::InputTag("hltEcalRecHit", "EcalRecHitsEE"));
  desc.add<bool>("skipHCAL", false);
  desc.add<bool>("skipECAL", false);
  //eThresHB/HE are from RecoParticleFlow/PFClusterProducer/python/particleFlowRecHitHBHE_cfi.py
  desc.add<std::vector<double> >("eThresHB", {0.1, 0.2, 0.3, 0.3});
  desc.add<std::vector<double> >("eThresHE", {0.1, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2});
  desc.add<double>("maxRapidity", 2.5);
  desc.add<double>("gridSpacing", 0.55);
  descriptions.addWithDefaultLabel(desc);
}

FixedGridRhoProducerFastjetFromRecHit::~FixedGridRhoProducerFastjetFromRecHit() = default;

void FixedGridRhoProducerFastjetFromRecHit::produce(edm::Event &iEvent, const edm::EventSetup &iSetup) {
  std::vector<fastjet::PseudoJet> inputs;
  auto const &thresholds = iSetup.getData(ecalPFRecHitThresholdsToken_);
  auto const &caloGeometry = iSetup.getData(caloGeometryToken_);

  if (!skipHCAL_) {
    auto const &hbheRecHits = iEvent.get(hbheRecHitsTag_);
    inputs.reserve(inputs.size() + hbheRecHits.size());
    for (const auto &hit : hbheRecHits) {
      if (passedHcalNoiseCut(hit)) {
        math::XYZTLorentzVector hitp4(0, 0, 0, 0);
        getHitP4(hit.id(), hit.energy(), hitp4, caloGeometry);
        inputs.push_back(fastjet::PseudoJet(hitp4.Px(), hitp4.Py(), hitp4.Pz(), hitp4.E()));
      }
    }
  }

  if (!skipECAL_) {
    auto const &ebRecHits = iEvent.get(ebRecHitsTag_);
    inputs.reserve(inputs.size() + ebRecHits.size());
    for (const auto &hit : ebRecHits) {
      if (passedEcalNoiseCut(hit, thresholds)) {
        math::XYZTLorentzVector hitp4(0, 0, 0, 0);
        getHitP4(hit.id(), hit.energy(), hitp4, caloGeometry);
        inputs.push_back(fastjet::PseudoJet(hitp4.Px(), hitp4.Py(), hitp4.Pz(), hitp4.E()));
      }
    }

    auto const &eeRecHits = iEvent.get(eeRecHitsTag_);
    inputs.reserve(inputs.size() + eeRecHits.size());
    for (const auto &hit : eeRecHits) {
      if (passedEcalNoiseCut(hit, thresholds)) {
        math::XYZTLorentzVector hitp4(0, 0, 0, 0);
        getHitP4(hit.id(), hit.energy(), hitp4, caloGeometry);
        inputs.push_back(fastjet::PseudoJet(hitp4.Px(), hitp4.Py(), hitp4.Pz(), hitp4.E()));
      }
    }
  }

  bge_.set_particles(inputs);
  iEvent.put(std::make_unique<double>(bge_.rho()));
}

void FixedGridRhoProducerFastjetFromRecHit::getHitP4(const DetId &detId,
                                                     float hitE,
                                                     math::XYZTLorentzVector &hitp4,
                                                     const CaloGeometry &caloGeometry) {
  const CaloSubdetectorGeometry *subDetGeom = caloGeometry.getSubdetectorGeometry(detId);
  if (subDetGeom != nullptr) {
    const auto &gpPos = subDetGeom->getGeometry(detId)->repPos();
    double thispt = hitE / cosh(gpPos.eta());
    double thispx = thispt * cos(gpPos.phi());
    double thispy = thispt * sin(gpPos.phi());
    double thispz = thispt * sinh(gpPos.eta());
    hitp4.SetPxPyPzE(thispx, thispy, thispz, hitE);
  } else {
    if (detId.rawId() != 0)
      edm::LogWarning("FixedGridRhoProducerFastjetFromRecHit")
          << "Geometry not found for a calo hit, setting p4 as (0,0,0,0)" << std::endl;
    hitp4.SetPxPyPzE(0, 0, 0, 0);
  }
}

//HCAL noise cleaning cuts.
bool FixedGridRhoProducerFastjetFromRecHit::passedHcalNoiseCut(const HBHERecHit &hit) {
  const auto thisDetId = hit.id();
  const auto thisDepth = thisDetId.depth();
  if (thisDetId.subdet() == HcalBarrel && hit.energy() > eThresHB_[thisDepth - 1])
    return true;
  else if (thisDetId.subdet() == HcalEndcap && hit.energy() > eThresHE_[thisDepth - 1])
    return true;
  return false;
}

//ECAL noise cleaning cuts using per-crystal PF-recHit thresholds.
bool FixedGridRhoProducerFastjetFromRecHit::passedEcalNoiseCut(const EcalRecHit &hit,
                                                               const EcalPFRecHitThresholds &thresholds) {
  return (hit.energy() > thresholds[hit.detid()]);
}

DEFINE_FWK_MODULE(FixedGridRhoProducerFastjetFromRecHit);
