/**
 * \file IIRFilter.h
 */

#ifndef ATK_EQ_IIRFILTER_H
#define ATK_EQ_IIRFILTER_H

#include <cassert>
#include <vector>

#include "config.h"

namespace ATK
{
  /// IIR filter template class (Direct Form I)
  template<class Coefficients >
  class ATK_EQ_EXPORT IIRFilter: public Coefficients
  {
  public:
    /// Simplify parent calls
    typedef Coefficients Parent;
    using typename Parent::DataType;
    using typename Parent::AlignedVector;
    using Parent::converted_inputs_size;
    using Parent::outputs_size;
    using Parent::converted_inputs;
    using Parent::outputs;
    using Parent::coefficients_in;
    using Parent::coefficients_out;
    using Parent::input_sampling_rate;
    using Parent::output_sampling_rate;
    using Parent::nb_input_ports;
    using Parent::nb_output_ports;
    
    using Parent::in_order;
    using Parent::out_order;
    using Parent::input_delay;
    using Parent::output_delay;
    using Parent::setup;
    
  public:
    /*!
     * @brief Constructor
     * @param nb_channels is the number of input and output channels
     */
    IIRFilter(int nb_channels = 1)
      :Parent(nb_channels)
    {
    }

    /// Move constructor
    IIRFilter(IIRFilter&& other)
    :Parent(std::move(other))
    {
    }

    void setup() override final
    {
      Parent::setup();
      input_delay = in_order;
      output_delay = out_order;

      if (out_order > 0)
      {
        coefficients_out_2.resize(out_order, 0);
        for (int i = 1; i < out_order; ++i)
        {
          coefficients_out_2[i] = coefficients_out[out_order - 1] * coefficients_out[i] + coefficients_out[i - 1];
        }
        coefficients_out_2[0] = coefficients_out[out_order - 1] * coefficients_out[0];
      }
      if (out_order > 1)
      {
        coefficients_out_3.resize(out_order, 0);
        for (int i = 0; i < 2; ++i)
        {
          coefficients_out_3[i] = coefficients_out[out_order - 2]  * coefficients_out[i] + coefficients_out[out_order - 1] * coefficients_out_2[i];
        }
        for (int i = 2; i < out_order; ++i)
        {
          coefficients_out_3[i] = coefficients_out[out_order - 2]  * coefficients_out[i] + coefficients_out[out_order - 1] * coefficients_out_2[i] + coefficients_out[i - 2];
        }
        if (out_order > 2)
        {
          coefficients_out_4.resize(out_order, 0);
          for (int i = 0; i < 3; ++i)
          {
            coefficients_out_4[i] = coefficients_out[out_order - 3]  * coefficients_out[i] + coefficients_out[out_order - 2] * coefficients_out_2[i] + coefficients_out[out_order - 1] * coefficients_out_3[i];
          }
          for (int i = 3; i < out_order; ++i)
          {
            coefficients_out_4[i] = coefficients_out[out_order - 3]  * coefficients_out[i] + coefficients_out[out_order - 2] * coefficients_out_2[i] + coefficients_out[out_order - 1] * coefficients_out_3[i] + coefficients_out[i - 3];
          }
        }
        else // out_order = 2
        {
          coefficients_out_4.resize(out_order, 0);
          for (int i = 0; i < 2; ++i)
          {
            coefficients_out_4[i] = coefficients_out[out_order - 2] * coefficients_out_2[i] + coefficients_out[out_order - 1] * coefficients_out_3[i];
          }
        }
      }
    }
    
    virtual void process_impl(int64_t size) const override final
    {
      assert(input_sampling_rate == output_sampling_rate);
      assert(nb_input_ports == nb_output_ports);
      assert(coefficients_in.data());
      assert(out_order == 0 || coefficients_out.data() != nullptr);

      const DataType* ATK_RESTRICT coefficients_in_ptr = coefficients_in.data();
      const DataType* ATK_RESTRICT coefficients_out_ptr = coefficients_out.data();
      const DataType* ATK_RESTRICT coefficients_out_2_ptr = coefficients_out_2.data();
      const DataType* ATK_RESTRICT coefficients_out_3_ptr = coefficients_out_3.data();
      const DataType* ATK_RESTRICT coefficients_out_4_ptr = coefficients_out_4.data();

      for(int channel = 0; channel < nb_input_ports; ++channel)
      {
        const DataType* ATK_RESTRICT input = converted_inputs[channel] - in_order;
        DataType* ATK_RESTRICT output = outputs[channel];

        for(int64_t i = 0; i < size; ++i)
        {
          output[i] = 0;
        }

        for (int j = 0; j < in_order + 1; ++j)
        {
          for (int64_t i = 0; i < size; ++i)
          {
            output[i] += coefficients_in_ptr[j] * input[i + j];
          }
        }

        int64_t i = 0;
        if (out_order > 2)
        {
          for (i = 0; i < size - 3; i += 4)
          {
            DataType tempout = output[i];
            DataType tempout2 = output[i] * coefficients_out_ptr[out_order - 1] + output[i + 1];
            DataType tempout3 = output[i] * coefficients_out_ptr[out_order - 2] + tempout2 * coefficients_out_ptr[out_order - 1] + output[i + 2];
            DataType tempout4 = output[i] * coefficients_out_ptr[out_order - 3] + tempout2 * coefficients_out_ptr[out_order - 2] + tempout3 * coefficients_out_ptr[out_order - 1] + output[i + 3];

            ATK_VECTORIZE_REMAINDER for (int j = 0; j < out_order; ++j)
            {
              tempout += coefficients_out_ptr[j] * output[i - out_order + j];
              tempout2 += coefficients_out_2_ptr[j] * output[i - out_order + j];
              tempout3 += coefficients_out_3_ptr[j] * output[i - out_order + j];
              tempout4 += coefficients_out_4_ptr[j] * output[i - out_order + j];
            }
            output[i] = tempout;
            output[i + 1] = tempout2;
            output[i + 2] = tempout3;
            output[i + 3] = tempout4;
          }
        }
        else if(out_order == 2)
        {
          for (i = 0; i < size - 3; i += 4)
          {
            DataType tempout = output[i];
            DataType tempout2 = output[i] * coefficients_out_ptr[out_order - 1] + output[i + 1];
            DataType tempout3 = output[i] * coefficients_out_ptr[out_order - 2] + tempout2 * coefficients_out_ptr[out_order - 1] + output[i + 2];
            DataType tempout4 = tempout2 * coefficients_out_ptr[out_order - 2] + tempout3 * coefficients_out_ptr[out_order - 1] + output[i + 3];

            ATK_VECTORIZE_REMAINDER for (int j = 0; j < out_order; ++j)
            {
              tempout += coefficients_out_ptr[j] * output[i - out_order + j];
              tempout2 += coefficients_out_2_ptr[j] * output[i - out_order + j];
              tempout3 += coefficients_out_3_ptr[j] * output[i - out_order + j];
              tempout4 += coefficients_out_4_ptr[j] * output[i - out_order + j];
            }
            output[i] = tempout;
            output[i + 1] = tempout2;
            output[i + 2] = tempout3;
            output[i + 3] = tempout4;
          }
        }
        for (; i < size; ++i)
        {
          DataType tempout = output[i];
          for (int j = 0; j < out_order; ++j)
          {
            tempout += coefficients_out_ptr[j] * output[i - out_order + j];
          }
          output[i] = tempout;
        }
      }
    }
    
    /// Returns the vector of internal coefficients for the MA section 
    const AlignedVector& get_coefficients_in() const
    {
      return coefficients_in;
    }
    
    /// Returns the vector of internal coefficients for the AR section, without degree 0 implicitely set to -1
    const AlignedVector& get_coefficients_out() const
    {
      return coefficients_out;
    }

  protected:
    AlignedVector coefficients_out_2;
    AlignedVector coefficients_out_3;
    AlignedVector coefficients_out_4;
  };

}

#endif
