import FWCore.ParameterSet.Config as cms

from ..modules.hltEG5EtL1SeededFilter_cfi import *
from ..modules.hltEgammaCandidatesL1Seeded_cfi import *
from ..modules.hltEgammaCandidatesWrapperL1Seeded_cfi import *
from ..modules.hltEgammaClusterShapeL1Seeded_cfi import *
from ..modules.hltEgammaEcalPFClusterIsoL1Seeded_cfi import *
from ..modules.hltEgammaEleGsfTrackIsoL1Seeded_cfi import *
from ..modules.hltEgammaEleGsfTrackIsoV6L1Seeded_cfi import *
from ..modules.hltEgammaEleL1TrkIsoL1Seeded_cfi import *
from ..modules.hltEgammaHcalPFClusterIsoL1Seeded_cfi import *
from ..modules.hltEgammaHGCALIDVarsL1Seeded_cfi import *
from ..modules.hltEgammaHGCalLayerClusterIsoL1Seeded_cfi import *
from ..modules.hltEgammaHoverEL1Seeded_cfi import *
from ..modules.hltEle5DphiL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightBestGsfChi2L1SeededFilter_cfi import *
from ..modules.hltEle5WPTightBestGsfNLayerITL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightClusterShapeL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightClusterShapeSigmavvL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightClusterShapeSigmawwL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightEcalIsoL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightGsfDetaL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightGsfDphiL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightGsfOneOEMinusOneOPL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightGsfTrackIsoFromL1TracksL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightGsfTrackIsoL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightHcalIsoL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightHEL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightHgcalHEL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightHgcalIsoL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightPixelMatchL1SeededFilter_cfi import *
from ..modules.hltEle5WPTightPMS2L1SeededFilter_cfi import *
from ..sequences.HLTDoFullUnpackingEgammaEcalL1SeededSequence_cfi import *
from ..sequences.HLTEGammaDoLocalHcalSequence_cfi import *
from ..sequences.HLTElePixelMatchL1SeededSequence_cfi import *
from ..sequences.HLTFastJetForEgammaSequence_cfi import *
from ..sequences.HLTGsfElectronL1SeededSequence_cfi import *
from ..sequences.HLTHgcalTiclPFClusteringForEgammaL1SeededSequence_cfi import *
from ..sequences.HLTL1Sequence_cfi import *
from ..sequences.HLTPFClusteringForEgammaL1SeededSequence_cfi import *
from ..sequences.HLTPFHcalClusteringForEgammaSequence_cfi import *
from ..sequences.HLTTrackingV61Sequence_cfi import *

HLTEle5OpenL1SeededSequence = cms.Sequence(HLTL1Sequence+HLTDoFullUnpackingEgammaEcalL1SeededSequence+HLTEGammaDoLocalHcalSequence+HLTPFClusteringForEgammaL1SeededSequence+HLTHgcalTiclPFClusteringForEgammaL1SeededSequence+HLTFastJetForEgammaSequence+HLTPFHcalClusteringForEgammaSequence+HLTElePixelMatchL1SeededSequence+HLTTrackingV61Sequence+HLTGsfElectronL1SeededSequence+(hltEgammaCandidatesL1Seeded+hltEgammaClusterShapeL1Seeded+hltEgammaHGCALIDVarsL1Seeded+hltEgammaHoverEL1Seeded+hltEgammaEcalPFClusterIsoL1Seeded+hltEgammaHGCalLayerClusterIsoL1Seeded+hltEgammaHcalPFClusterIsoL1Seeded+hltEgammaEleL1TrkIsoL1Seeded+hltEgammaEleGsfTrackIsoL1Seeded+hltEgammaEleGsfTrackIsoV6L1Seeded)+hltEgammaCandidatesWrapperL1Seeded+hltEG5EtL1SeededFilter+hltEle5DphiL1SeededFilter+cms.ignore(hltEle5WPTightClusterShapeL1SeededFilter)+cms.ignore(hltEle5WPTightClusterShapeSigmavvL1SeededFilter)+cms.ignore(hltEle5WPTightClusterShapeSigmawwL1SeededFilter)+cms.ignore(hltEle5WPTightHgcalHEL1SeededFilter)+cms.ignore(hltEle5WPTightHEL1SeededFilter)+cms.ignore(hltEle5WPTightEcalIsoL1SeededFilter)+cms.ignore(hltEle5WPTightHgcalIsoL1SeededFilter)+cms.ignore(hltEle5WPTightHcalIsoL1SeededFilter)+cms.ignore(hltEle5WPTightPixelMatchL1SeededFilter)+cms.ignore(hltEle5WPTightPMS2L1SeededFilter)+cms.ignore(hltEle5WPTightGsfOneOEMinusOneOPL1SeededFilter)+cms.ignore(hltEle5WPTightGsfDetaL1SeededFilter)+cms.ignore(hltEle5WPTightGsfDphiL1SeededFilter)+cms.ignore(hltEle5WPTightBestGsfNLayerITL1SeededFilter)+cms.ignore(hltEle5WPTightBestGsfChi2L1SeededFilter)+cms.ignore(hltEle5WPTightGsfTrackIsoFromL1TracksL1SeededFilter)+cms.ignore(hltEle5WPTightGsfTrackIsoL1SeededFilter))
