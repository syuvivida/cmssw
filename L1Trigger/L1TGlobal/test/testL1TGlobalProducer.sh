#!/bin/bash

# Pass in name and status
function die {
  echo $1: status $2
  echo === Log file ===
  cat ${3:-/dev/null}
  echo === End log file ===
  exit $2
}

# run test job
TESTDIR="${LOCALTOP}"/src/L1Trigger/L1TGlobal/test

cmsRun "${TESTDIR}"/testL1TGlobalProducer_cfg.py &> log_testL1TGlobalProducer \
 || die "Failure running testL1TGlobalProducer_cfg.py" $? log_testL1TGlobalProducer

# expected PathSummary of test job
cat <<@EOF > log_testL1TGlobalProducer_expected
    Bit                  Algorithm Name                  Init    PScd  Final   PS Factor     Num Bx Masked
============================================================================================================
      21                              L1_SingleMu22       228    228    228         1          0
     194                      L1_SingleIsoEG32er2p5       244      0      0         0          0
     286                 L1_Mu22er2p1_IsoTau30er2p1       160    133    133       1.2          0
     318                        L1_SingleJet90er2p5       686    103    103      6.65          0
     428                         L1_ETMHF80_HTT60er       155     45     45       3.4          0
     459                                L1_ZeroBias      1000     19     19      50.1          0
     461              L1_MinimumBiasHF0_AND_BptxAND       983    799    799      1.23          0
     480                   L1_FirstCollisionInOrbit         0      0      0         1          0
                                                      Final OR Count = 866
@EOF

# compare to expected output of test job
sed -n '/Init    PScd  Final   PS Factor/,/Final OR Count =/p' log_testL1TGlobalProducer \
 | diff log_testL1TGlobalProducer_expected - \
 || die "differences in expected log report" $?
