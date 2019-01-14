/*
 * Copyright (C) 2005-2017 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef otbLocalRxDetectorFilter_h
#define otbLocalRxDetectorFilter_h

#include "itkImageToImageFilter.h"

#include "itkConstShapedNeighborhoodIterator.h"
#include "itkNeighborhoodAlgorithm.h"
#include "itkImageRegionIterator.h"
#include "itkListSample.h"
#include "itkCovarianceSampleFilter.h"
#include "itkProgressReporter.h"

namespace otb
{

/** \class otbLocalRxDetectorFilter
 * \brief Local-RX detector algorithm with multichannel VectorImage data as input
 *
 *
 * \ingroup ImageFilters
 *
 * \ingroup OTBAnomalyDetection
 */
template <class TInputImage, class TOutputImage>
class ITK_EXPORT LocalRxDetectorFilter:
public itk::ImageToImageFilter<TInputImage, TOutputImage>
{
public:

  /** Standard class typedefs. */
  typedef LocalRxDetectorFilter                                                     Self;
  typedef itk::ImageToImageFilter< TInputImage, TOutputImage >                               Superclass;
  typedef itk::SmartPointer<Self>                                                         Pointer;
  typedef itk::SmartPointer<const Self>                                             ConstPointer;

  /** Type macro */
  itkNewMacro(Self);

  /** Creation through object factory macro */
  itkTypeMacro(LocalRxDetectorFilter, ImageToImageFilter);

  /** typedef related to input and output images */
  typedef TInputImage                            InputImageType;
  typedef typename InputImageType::Pointer       InputPointerType;
  typedef typename InputImageType::ConstPointer  InputConstPointerType;
  typedef typename InputImageType::IndexType     InputIndexType;
  typedef typename InputImageType::SizeType      InputSizeType;

  typedef TOutputImage                           OutputImageType;
  typedef typename OutputImageType::Pointer      OutputPointerType;
  typedef typename OutputImageType::IndexType    OutputIndexType;
  typedef typename OutputImageType::OffsetType   OutputOffsetType;
  typedef typename OutputImageType::SizeType     OutputSizeType;
  typedef typename OutputImageType::RegionType   OutputImageRegionType;

  /** typedef related to iterators */
  typedef itk::ConstShapedNeighborhoodIterator<InputImageType>                             ConstShapedNeighborhoodIteratorType;
  typedef itk::NeighborhoodAlgorithm::ImageBoundaryFacesCalculator<InputImageType>        VectorFaceCalculatorType;
  typedef itk::NeighborhoodAlgorithm::ImageBoundaryFacesCalculator<OutputImageType>        FaceCalculatorType;
  typedef itk::ImageRegionIterator<OutputImageType>                                                                ImageRegionIteratorType;

  /** typedef related to statistics */
  typedef typename InputImageType::PixelType                      VectorMeasurementType;
  typedef itk::Statistics::ListSample<VectorMeasurementType>      ListSampleType;
  typedef itk::Statistics::CovarianceSampleFilter<ListSampleType> CovarianceCalculatorType;
  typedef typename CovarianceCalculatorType::MeasurementVectorRealType MeasurementVectorRealType;
  typedef typename CovarianceCalculatorType::MatrixType                MatrixType;

  /** Getter and Setter */
  itkSetMacro(InternalRadius, int);
  itkGetMacro(InternalRadius, int);
  itkSetMacro(ExternalRadius, int);
  itkGetMacro(ExternalRadius, int);

  /** Main computation method */
  void GenerateInputRequestedRegion() override;
//       virtual void GenerateData();
  void BeforeThreadedGenerateData() override;
  void ThreadedGenerateData(const OutputImageRegionType& outputRegionForThread, itk::ThreadIdType threadId) override;


protected:
  LocalRxDetectorFilter();
  ~LocalRxDetectorFilter() override {}
  void PrintSelf(std::ostream& os, itk::Indent indent) const override;

private:
  LocalRxDetectorFilter(const Self&) = delete;
  void operator=(const Self&) = delete;

  int m_InternalRadius;
  int m_ExternalRadius;

};


template<typename T> 
class localRxDetectionFunctor
{
public:

  /** typedef */
  typedef typename itk::Neighborhood<itk::VariableLengthVector<T>>::OffsetType OffsetType;
  typedef typename itk::VariableLengthVector<T>                         VectorMeasurementType;
  typedef itk::Statistics::ListSample<VectorMeasurementType>            ListSampleType;
  typedef itk::Statistics::CovarianceSampleFilter<ListSampleType>       CovarianceCalculatorType;
  typedef typename CovarianceCalculatorType::MeasurementVectorRealType  MeasurementVectorRealType;
  typedef typename CovarianceCalculatorType::MatrixType                 MatrixType;

private:
  int m_InternalRadius;


public:
  localRxDetectionFunctor():m_InternalRadius(1){};

  void SetInternalRadius(int internalRadius)
  {
    m_InternalRadius = internalRadius;
  };

  int GetInternalRadius()
  {
    return m_InternalRadius;
  };


  auto operator()(const itk::Neighborhood<itk::VariableLengthVector<T>> & in) const
  {
    // Create a list sample with the pixels of the neighborhood located between
    // the two radius.
    typename ListSampleType::Pointer listSample = ListSampleType::New();

    // The pixel on whih we will compute the Rx score, we load it now to get the input vector size.
    auto centerPixel = in.GetCenterValue();
    listSample->SetMeasurementVectorSize(centerPixel.Size());

    OffsetType off;
    auto externalRadius = in.GetRadius();
    for (int y = -static_cast<int>(externalRadius[1]); y <= static_cast<int>(externalRadius[1]); y++)
      {
      off[1] = y;
      for (int x = -static_cast<int>(externalRadius[0]); x <= static_cast<int>(externalRadius[0]); x++)
        {
        off[0] = x;
        if ((abs(x) > m_InternalRadius) || (abs(y) > m_InternalRadius))
          {//std::cout << in[off] << std::endl;
            listSample->PushBack(in[off] );
          }
        }
      }

    // Compute mean & inverse covariance matrix
    typename CovarianceCalculatorType::Pointer covarianceCalculator = CovarianceCalculatorType::New();
    covarianceCalculator->SetInput(listSample);
    covarianceCalculator->Update();

    MeasurementVectorRealType meanVector = covarianceCalculator->GetMean();
    
    VectorMeasurementType meanVec(meanVector.GetNumberOfElements());
    for(unsigned int i = 0; i < meanVector.GetNumberOfElements(); ++i)
      {
      meanVec.SetElement(i, meanVector.GetElement(i));
      }

    MatrixType covarianceMatrix = covarianceCalculator->GetCovarianceMatrix();
    typename MatrixType::InternalMatrixType invCovMat = covarianceMatrix.GetInverse();

    typename MatrixType::InternalMatrixType centeredTestPixMat(meanVector.GetNumberOfElements(), 1);
    
    for (unsigned int i = 0; i < centeredTestPixMat.rows(); ++i)
      {
      centeredTestPixMat.put(i, 0, (centerPixel.GetElement(i) - meanVector.GetElement(i)));
      }

    // Rx score computation
    typename MatrixType::InternalMatrixType rxValue 
      = centeredTestPixMat.transpose() * invCovMat * centeredTestPixMat;

    return rxValue.get(0, 0);
  }

};



} // end namespace otb

#ifndef OTB_MANUAL_INSTANTIATION
#include "otbLocalRxDetectorFilter.hxx"
#endif

#endif
