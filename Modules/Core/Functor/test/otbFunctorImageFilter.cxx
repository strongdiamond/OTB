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

#include "itkMacro.h"
#include "otbFunctorImageFilter.h"
#include "otbImage.h"
#include "otbVectorImage.h"
#include "itkNeighborhood.h"
#include "otbVariadicAddFunctor.h"
#include "otbVariadicConcatenateFunctor.h"
#include <tuple>

#include <numeric>
#include <complex>

// static tests

// Tests of IsNeighborhood, PixelTypeDeduction and ImageTypeDeduction structs
using namespace otb;
template <typename T> struct TypesCheck
{
  using ScalarType                    = T;
  using ImageType                     = otb::Image<T>;
  using VectorType                    = itk::VariableLengthVector<ScalarType>;
  using VectorImageType               = otb::VectorImage<ScalarType>;
  using NeighborhoodType              = itk::Neighborhood<ScalarType>;
  using VectorNeighborhoodType        = itk::Neighborhood<VectorType>;
  
  // Test IsNeighborhood struct
  static_assert(!otb::IsNeighborhood<ScalarType>::value,"");
  static_assert(!otb::IsNeighborhood<VectorType>::value,"");
  static_assert(otb::IsNeighborhood<NeighborhoodType>::value,"");
  static_assert(otb::IsNeighborhood<VectorNeighborhoodType>::value,"");
  static_assert(otb::IsNeighborhood<const NeighborhoodType &>::value,"");
  static_assert(otb::IsNeighborhood<const VectorNeighborhoodType &>::value,"");

  // Test PixelTypeDeduction struct
  static_assert(std::is_same<typename PixelTypeDeduction<ScalarType>::PixelType,ScalarType>::value,"");
  static_assert(std::is_same<typename PixelTypeDeduction<VectorType>::PixelType,VectorType>::value,"");
  static_assert(std::is_same<typename PixelTypeDeduction<NeighborhoodType>::PixelType,ScalarType>::value,"");
  static_assert(std::is_same<typename PixelTypeDeduction<VectorNeighborhoodType>::PixelType,VectorType>::value,"");
  static_assert(std::is_same<typename PixelTypeDeduction<const NeighborhoodType &>::PixelType,ScalarType>::value,"");
  static_assert(std::is_same<typename PixelTypeDeduction<const VectorNeighborhoodType &>::PixelType,VectorType>::value,"");

  // Test ImageTypeDeduction struct
  static_assert(std::is_same<typename ImageTypeDeduction<ScalarType>::ImageType,ImageType>::value,"");
  static_assert(std::is_same<typename ImageTypeDeduction<VectorType>::ImageType,VectorImageType>::value,"");
  static_assert(std::is_same<typename ImageTypeDeduction<const ScalarType &>::ImageType,ImageType>::value,"");
  static_assert(std::is_same<typename ImageTypeDeduction<const VectorType &>::ImageType,VectorImageType>::value,"");

  // Fake test operator
  template <typename TOut,typename TIn> struct TestOperator
  {
    auto operator()(const TIn&) const
    {
      TOut res(OutputSize());
      return res;
    }
    
    constexpr size_t OutputSize(...) const
    {
      return 1;
    }
  };

    
  template <typename TOut, typename TIn> void TestFilter()
  {
  // Deduce types
  using InputPixelType = typename PixelTypeDeduction<TIn>::PixelType;
  using InputImageType = typename ImageTypeDeduction<InputPixelType>::ImageType;

  // Allocate fake input
  auto in = InputImageType::New();
  typename InputImageType::SizeType size = {{10,10}};
  in->SetRegions(size);
  in->SetNumberOfComponentsPerPixel(1);
  in->Allocate();
  InputPixelType defaultValue(1);
  in->FillBuffer(defaultValue);

  // Build and run filter
  auto functor = TestOperator<TOut,TIn>{};
  auto filter = NewFunctorFilter(functor);

  using FilterType = typename decltype(filter)::ObjectType;
  static_assert(std::is_same<typename std::tuple_element<0, typename FilterType::Superclass::InputTypesTupleType>::type, InputImageType>::value, "");

  filter->SetVInputs(in);
  filter->SetInput1(in);
  filter->template SetVInput<0>(in); // template keyword to avoid C++ parse ambiguity
  filter->Update();
  }

  TypesCheck()
  {
  // Test possible combinations
  TestFilter<ScalarType,ScalarType>();
  TestFilter<ScalarType,VectorType>();
  TestFilter<ScalarType,NeighborhoodType>();
  TestFilter<VectorType,ScalarType>();
  } 
};

auto checksDouble = TypesCheck<double>{};
auto checksComplex = TypesCheck<std::complex<double>>{};

// Example functors



// 1 VectorImage -> 1 VectorImage with a different size depending on a
// parameter of the functor
// This Functor 
template<typename TOut, typename TIn> struct BandExtraction
{
  BandExtraction(unsigned int indices...) : m_Indices({indices})
  {}
  
  // TODO define a constructor to initialize m_Indices  
  auto operator()(const itk::VariableLengthVector<TIn> & in) const
  {
    itk::VariableLengthVector<TOut> out(m_Indices.size());

    size_t idx = 0;
    for(auto v: m_Indices)
      {
      out[idx] = static_cast<TOut>(in[v]);
      ++idx;
      }

    return out;
  }

  // This time OutputSize does not depend on input image size, hence
  // the ...
  size_t OutputSize(...) const
  {
    return m_Indices.size();
  }
  
  // set of band indices to extract
  std::set<unsigned int> m_Indices;
};

// 1 Image with neighborhood -> 1 Image
// This Functor computes the mean in neighborhood
template<typename TOut, typename TIn> struct Mean
{  
  auto operator()(const itk::Neighborhood<TIn> & in) const
  {
    TOut out(0);

    for(auto it = in.Begin(); it!=in.End();++it)
      out+=static_cast<TOut>(*it);

    out/=in.Size();
    
    return out;
  }
};

// 1 Image with neighborhood of VariableLengthVector -> 1 image with
// VariableLengthVector
// For each channel, returns the maximum value in neighborhood
template<typename T> struct MaxInEachChannel
{
  auto operator()(const itk::Neighborhood<itk::VariableLengthVector<T>> & in) const
  {
    auto out = in.GetCenterValue();

    for(auto it = in.Begin(); it!=in.End(); ++it)
      {
      for(auto band = 0u; band < out.Size();++band)
        {
        if((*it)[band]>out[band])
          out[band] = (*it)[band];
        }
      }
    return out;
  }

  size_t OutputSize(const std::array<size_t,1> & nbBands) const
  {
    return nbBands[0];
  } 
};


template<typename T> struct VectorModulus
{
  itk::VariableLengthVector<double> operator()(const itk::VariableLengthVector<std::complex<T>> & in) const
  {
    itk::VariableLengthVector<double> out(in.Size());
    
    for(auto band = 0u; band < out.Size(); ++band)
      {
      out[band] = std::abs(in[band]);
      }
    return out;
  }

  size_t OutputSize(const std::array<size_t,1> & nbBands) const
  {
    return nbBands[0];
  }
};

int otbFunctorImageFilter(int itkNotUsed(argc), char * itkNotUsed(argv) [])
{
  // test functions in functor_filter_details namespace
  using VectorImageType = VectorImage<double>;
  using ImageType       = Image<double>;
  using ComplexVectorImageType = VectorImage<std::complex<double>>;
  using ComplexImageType = Image<std::complex<double>>;
  using RegionType      = typename ImageType::RegionType;
  using SizeType        = typename RegionType::SizeType;
  
  auto vimage = VectorImageType::New();
  auto image  = ImageType::New();
  auto cvimage = ComplexVectorImageType::New();
  auto cimage = ComplexImageType::New();

  SizeType size = {{200,200}};
  
  vimage->SetRegions(size);
  vimage->SetNumberOfComponentsPerPixel(2);
  vimage->Allocate();
  itk::VariableLengthVector<double> v(2);
  v.Fill(0);
  vimage->FillBuffer(v);

  cvimage->SetRegions(size);
  cvimage->SetNumberOfComponentsPerPixel(2);
  cvimage->Allocate();
  itk::VariableLengthVector<std::complex<double>> cv(2);
  cv.Fill(0);
  cvimage->FillBuffer(cv);
  
  image->SetRegions(size);
  image->Allocate();
  image->FillBuffer(0);

  cimage->SetRegions(size);
  cimage->Allocate();
  cimage->FillBuffer(0);

  // Test VariadicInputsImageFilter
  auto filter = otb::VariadicInputsImageFilter<VectorImageType,VectorImageType,ImageType>::New();
  filter->SetVInput<0>(vimage);
  filter->SetVInput<1>(image);

  filter->SetInput1(vimage);
  filter->SetInput2(image);

  filter->SetVInputs(vimage,image);
  std::cout<<filter->GetVInput<0>()<< filter->GetVInput<1>()<<std::endl;

  
  // test FunctorImageFilter with a lambda
  double scale = 10.;  
  auto Lambda1 = [scale](double p)
               {
                 return scale*p;
               };
  auto filterLambda = NewFunctorFilter(Lambda1);
  filterLambda->SetVInputs(image);
  filterLambda->Update();

  // test FunctorImageFilter with a lambda that returns a
  // VariableLengthVector
  // Converts a neighborhood to a VariableLengthVector
  auto Lambda2 = [](const itk::Neighborhood<double> & in)
                 {
                   itk::VariableLengthVector<double> out(in.Size());
                   std::size_t idx{0};
                   for(auto it = in.Begin(); it!=in.End();++it,++idx)
                     {
                     out[idx]=*it;
                     }
                   return out;
                 };

  // In this case, we use the helper function which allows to specify
  // the number of outputs
  auto filterLambda2  = NewFunctorFilter(Lambda2,vimage->GetNumberOfComponentsPerPixel(),{{3,3}});
  filterLambda2->SetVInputs(image);
  filterLambda2->Update();
  
  // Test FunctorImageFilter with the VariadicConcatenate operator
  using ConcatFunctorType = Functor::VariadicConcatenate<double, double, itk::VariableLengthVector<double> >;
  auto concatenate = NewFunctorFilter(ConcatFunctorType{});
  concatenate->SetVInputs(image,vimage);
  concatenate->Update();
  
  // Test FunctorImageFilter With VariadicAdd functor
  using AddFunctorType = Functor::VariadicAdd<double, double, double>;
  auto add = NewFunctorFilter(AddFunctorType{});
  add->SetVInputs(image,image);
  add->Update();

  // Test FunctorImageFilter with BandExtraction functor
  using ExtractFunctorType = BandExtraction<double,double>;
  ExtractFunctorType extractFunctor{1,2};
  auto extract = NewFunctorFilter(extractFunctor);
  extract->SetVInputs(vimage);
  extract->Update();
  
  // Test FunctorImageFilter With Mean functor
  using MeanFunctorType = Mean<double,double>;
  auto median = NewFunctorFilter(MeanFunctorType{},{{2,2}});
  median->SetVInputs(image);
  median->Update();

  // Test FunctorImageFilter with MaxInEachChannel
  using MaxInEachChannelType = MaxInEachChannel<double>;
  auto maxInEachChannel = NewFunctorFilter(MaxInEachChannelType{},{{3,3}});
  maxInEachChannel->SetVInputs(vimage);
  maxInEachChannel->Update();

  // Test FunctorImageFilter with Module (complex=
  using ModulusType = VectorModulus<double>;
  auto modulus = NewFunctorFilter(ModulusType{});
  modulus->SetVInputs(cvimage);
  modulus->Update();

  auto LambdaComplex = [] (const std::complex<double> & in) {return std::arg(in);};
  auto argFilter = NewFunctorFilter(LambdaComplex);
  argFilter->SetVInputs(cimage);
  argFilter->Update();

 return EXIT_SUCCESS;
}
