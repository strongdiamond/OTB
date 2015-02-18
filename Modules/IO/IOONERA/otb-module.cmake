set(DOCUMENTATION "This module contains features to read ONERA format images.")

otb_module(OTBIOONERA 
  DEPENDS
    OTBImageBase
    OTBCommon
  TEST_DEPENDS
    OTBTestKernel
  
  DESCRIPTION
    "${DOCUMENTATION}"
  )
