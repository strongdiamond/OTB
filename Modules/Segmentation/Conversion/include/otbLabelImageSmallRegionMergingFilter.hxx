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


#ifndef otbLabelImageSmallRegionMergingFilter_hxx
#define otbLabelImageSmallRegionMergingFilter_hxx

#include "otbLabelImageSmallRegionMergingFilter.h"
#include "itkImageConstIterator.h"
#include "itkConstShapedNeighborhoodIterator.h"
#include "itkProgressReporter.h"

#include <time.h>
namespace otb
{
template <class TInputLabelImage >
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::PersistentLabelImageSmallRegionMergingFilter() : m_Size(1)
{
}

template <class TInputLabelImage >
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::~PersistentLabelImageSmallRegionMergingFilter()
{
}


template <class TInputLabelImage >
void
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::Reset()
{
  m_NeighboursMapsTmp.clear();
  m_NeighboursMapsTmp.resize( this->GetNumberOfThreads() );
}


template <class TInputLabelImage >
void
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::Synthetize()
{
  NeigboursMapType neighboursMap;
  // Merge the neighbours maps from all threads
  for( unsigned int threadId = 0; threadId < this->GetNumberOfThreads(); threadId++)
  {
    for (auto it = m_NeighboursMapsTmp[threadId].begin(); it != m_NeighboursMapsTmp[threadId].end(); it++)
    {
      neighboursMap[ it->first ].insert( it->second.begin(), it->second.end() );
    }
  }
  
  // For each label of the label map, find the "closest" connected label, according 
  // to the euclidian distance between the corresponding m_labelStatistic elements.
  for (auto neighbours : neighboursMap)
  {
    double proximity = std::numeric_limits<double>::max();
    InputLabelType label = neighbours.first;
    InputLabelType closestNeighbour = label;
    for (auto neighbour : neighbours.second)
    {
      
      auto statsLabel = m_LabelStatistic[ label ];
      auto statsNeighbour = m_LabelStatistic[ neighbour ];
      assert( statsLabel.Size() == statsNeighbour.Size() );
      double distance = 0;
      for (unsigned int i = 0 ; i < statsLabel.Size(); i++)
      {
        distance += pow( statsLabel[i] - statsNeighbour[i] , 2);
      }
      if (distance < proximity)
      {
        proximity = distance;
        closestNeighbour = neighbour;
      }
    }

    auto curLabelLUT = label;
    auto adjLabelLUT = closestNeighbour;
    std::cout << label << " " << closestNeighbour;
    while(m_LUT[curLabelLUT] != curLabelLUT)
    {
      curLabelLUT = m_LUT[curLabelLUT];
    }
    while(m_LUT[adjLabelLUT] != adjLabelLUT)
    {
      adjLabelLUT = m_LUT[adjLabelLUT];
    }
    
    if(curLabelLUT < adjLabelLUT)
    {
      m_LUT[adjLabelLUT] = curLabelLUT;
    }
    else
    {
      m_LUT[m_LUT[curLabelLUT]] = adjLabelLUT; 
      m_LUT[curLabelLUT] = adjLabelLUT;
    }
  }
  
  for(InputLabelType label = 0; label < m_LUT.size(); ++label)
  {
    InputLabelType can = label;
    while(m_LUT[can] != can)
    {
      can = m_LUT[can];
    }
  m_LUT[label] = can;
  }

  
  for(InputLabelType label = 0; label < m_LUT.size(); ++label)
  {
    InputLabelType correspondingLabel = m_LUT[label];
    
    if((m_LabelPopulation[label]!=0) && (correspondingLabel != label))
    {
      m_LabelStatistic[ correspondingLabel ] = (m_LabelStatistic[correspondingLabel]*m_LabelPopulation[correspondingLabel] + 
                        m_LabelStatistic[label]*m_LabelPopulation[label] ) / (m_LabelPopulation[label]+m_LabelPopulation[correspondingLabel]);
      m_LabelPopulation[ correspondingLabel ] += m_LabelPopulation[ label ] ;
      m_LabelPopulation[ label ] = 0;
    }
    
  }
}

template <class TInputLabelImage >
typename PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >::InputLabelType
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::FindCorrespondingLabel( typename PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
                            ::InputLabelType label)
{
  auto correspondingLabel = m_LUT[label];
  while (label != correspondingLabel)
  {
    label = correspondingLabel;
    correspondingLabel = m_LUT[correspondingLabel];
  }
  return correspondingLabel;
}



template <class TInputLabelImage >
void
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::GenerateOutputInformation()
{
  Superclass::GenerateOutputInformation();
}

template <class TInputLabelImage >
void
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::ThreadedGenerateData(const RegionType& outputRegionForThread, itk::ThreadIdType threadId )
{ 
  using IteratorType = itk::ImageRegionConstIterator< TInputLabelImage >;
  using NeighborhoodIteratorType = itk::ConstShapedNeighborhoodIterator< TInputLabelImage >;
  
  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill(1);

  auto labelImage = this->GetInput();
  
  IteratorType it(labelImage, outputRegionForThread);
  NeighborhoodIteratorType itN(radius, labelImage, outputRegionForThread);
  
  // 4 connected Neighborhood (top, bottom, left and right)
  typename IteratorType::OffsetType top = {{0,-1}};
  itN.ActivateOffset(top);
  typename IteratorType::OffsetType bottom = {{0,1}};
  itN.ActivateOffset(bottom);
  typename IteratorType::OffsetType right = {{1,0}};
  itN.ActivateOffset(right);
  typename IteratorType::OffsetType left = {{-1,0}};
  itN.ActivateOffset(left);
  
  for (it.GoToBegin(); ! it.IsAtEnd(); ++it, ++itN)
  {
    assert( !itN.IsAtEnd() );
    int currentLabel = FindCorrespondingLabel(it.Get());
    
    if ( m_LabelPopulation[currentLabel] == m_Size )
    {
      for (auto ci = itN.Begin() ; !ci.IsAtEnd(); ci++)
      {
        int neighbourLabel = FindCorrespondingLabel(ci.Get() );
        if (neighbourLabel != currentLabel)
          m_NeighboursMapsTmp[threadId][ currentLabel ].insert( neighbourLabel );
      }
    }
  }
}

template <class TInputLabelImage >
void
PersistentLabelImageSmallRegionMergingFilter< TInputLabelImage >
::PrintSelf(std::ostream& os, itk::Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

template <class TInputLabelImage >
LabelImageSmallRegionMergingFilter< TInputLabelImage >
::LabelImageSmallRegionMergingFilter() : m_MinSize(1)
{
  m_SmallRegionMergingFilter = LabelImageSmallRegionMergingFilterType::New();
}

template <class TInputLabelImage >
void
LabelImageSmallRegionMergingFilter< TInputLabelImage >
::GenerateData()
{
  auto labelImage = this->GetInput();
  m_SmallRegionMergingFilter->GetFilter()->SetInput( labelImage );
  
  for (unsigned int size = 1; size < m_MinSize; size++)
  {
    m_SmallRegionMergingFilter->GetFilter()->SetSize( size) ;
    m_SmallRegionMergingFilter->Update();
  }
}


} // end namespace otb

#endif
