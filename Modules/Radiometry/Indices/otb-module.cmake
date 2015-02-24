set(DOCUMENTATION "With multispectral sensors, several indices can be computed
combining several spectral bands to show features that are not obvious using only
one band. This module contains filters that can compute classical indices, such
as NDVI (Normalized Difference Vegetation Index), NDWI (Normalized
DifferenceWater Index) and so on.")

otb_module(OTBIndices
  DEPENDS
    OTBVectorDataBase
    OTBITK
    OTBITKPendingPatches
    OTBImageManipulation
    OTBFuzzy
    OTBPath
    OTBCommon
    OTBMetadata

  TEST_DEPENDS
    OTBTestKernel
    OTBImageIO
    OTBProjection
    OTBVectorDataIO
    OTBImageBase
    OTBObjectList

  DESCRIPTION
    "${DOCUMENTATION}"
)
