// Copyright 2017 University of Bristol
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <pmmintrin.h>
#include "matrix_utils.h"

#define VERBOSE 0
/*
TODO:
	* Add flag/switch for format of input matrix
		** Completely filled
			*** Run compress() to format for use in exponentiation
		** Compressed
			*** Accept directly from Python in CSC/CSR format 
			*** Place in class elements here for mul_hermitian and exponentiation utilities. 

	* Possibly have function hamiltonian_exponentiation(input_format)
		** input_format = { completely_filled, python_scipy_csc, python_scipy_csr, etc}
*/


void ComplexMatrix::mag_sqr(RealMatrix& dst) const
{
    const complex_t* psrc = get_row(0);
    double* pdst = dst.get_row(0);
    for (uint32_t i = 0; i < num_values; ++i)
        *pdst++ = ::mag_sqr(*psrc++);
}

void ComplexMatrix::make_identity()
{
    complex_t zero = to_complex(0.0, 0.0);
    complex_t one = to_complex(1.0, 0.0);
    for (uint32_t row = 0; row < num_rows; ++row)
    {
        complex_t* prow = get_row(row);
        for (uint32_t col = row; col < num_cols; ++col)
        {
            complex_t* pcol = get_row(col);
            if (row == col)
                prow[col] = pcol[row] = one;
            else
                prow[col] = pcol[row] = zero;
        }
    }
}

/*
void ComplexMatrix::make_zero()
{
    complex_t zero = to_complex(0.0, 0.0);
    for (uint32_t row = 0; row < num_rows; ++row)
    {
        complex_t* prow = get_row(row);
        for (uint32_t col = row; col < num_cols; ++col)
        {
            complex_t* pcol = get_row(col);
            prow[col] = pcol[row] = zero;
        }
    }
}
*/


void ComplexMatrix::make_zero()
{
    complex_t zero = to_complex(0.0, 0.0);
    for (uint32_t i = 0; i < num_cols*num_rows; ++i)
    {
			values[i] = zero;
    }
}



void ComplexMatrix::print_compressed_storage() const
{
	printf("Max nnz in any row : %u \n", max_nnz_in_a_row);

	for(uint32_t i=0; i<num_rows; i++){
		printf("Nonzeros in row %u :\n", i);
		for(uint32_t j=0; j< num_nonzeros_by_row[i]; j++){
			printf("Loc :%u, \t", nonzero_col_locations[i][j]);
			complex_t val = nonzero_values[i][j];
			printf("Val :%.2f + %.2f i, \n", get_real(val), get_imag(val));
		}
	}

}

#define OPT_3 0 // opt 3 correctly exploits Symmetrical shape, but not sparsity. 
#define OPT_4 1 // opt 4 used for development of sparsity utility
#define mul_full 0

#define testing_class 0
#define print_mul_hermitian 0

// dst = this * rhs
void ComplexMatrix::mul_hermitian(const ComplexMatrix& rhs, ComplexMatrix& dst) // changing const of rhs
{
    // TODO: take advantage of the fact that these are diagonally semmetrical
    size_t size = num_rows;
    complex_t zero = to_complex(0.0, 0.0);
    complex_t conj = to_complex(1.0, -1.0);

		/*
		printf("Multiplying this : \n");
		this->debug_print();
		printf("by this : \n");
		rhs.debug_print();
		//*/
		
#if OPT_4
		// printf("OPT 4 \n");
		dst.make_zero();
    for (uint32_t row = 0; row < size; ++row)
    {
			if(this->num_nonzeros_by_row[row] != 0)
			{
				complex_t* dst_row = dst.get_row(row);
//			    const complex_t* src1 = get_row(row);
				for (uint32_t col = row; col<size; ++col)
				{
					if (rhs.num_nonzeros_by_row[col] != 0) 
					{
				  //	const complex_t* src2 = rhs.get_row(col);
						complex_t accum = zero;
			  		
						for (uint32_t j = 0; j < this -> max_nnz_in_a_row; j++)
						{
							uint32_t col_loc = this->nonzero_col_locations[row][j];
							if(col_loc != num_cols)
							{
								for (uint32_t k=0; k < rhs.max_nnz_in_a_row; k++)
								{
									if (col_loc == rhs.nonzero_col_locations[col][k])
									{
										accum = add(accum, mul(this->nonzero_values[row][j], rhs.nonzero_values[col][k]*conj ));					
									}					
								}
							}
						}
						dst_row[col] = accum;
						dst[col][row] = accum * conj; 
					}
					}
				} // end for (row) loop

		}

#elif OPT_3

		for (uint32_t row = 0; row < size; ++row)
		{
      // Here we make the assumption that the output will be symmetric across
      // the diagonal, so we only need to calculate half of it, and  then
      // write the other half to match.
      complex_t* dst_row = dst.get_row(row);
      const complex_t* src1 = get_row(row);
      for (uint32_t col = row; col < size; ++col)
      {
          const complex_t* src2 = rhs.get_row(col);
          complex_t accum = zero;
          for (uint32_t i = 0; i < size; ++i)
              accum = add(accum, mul(src1[i], src2[i] * conj));
          dst_row[col] = accum;
          dst[col][row] = accum * conj; // This * conj may belong on the previous line
      }
    } // end for (row) loop
#elif mul_full
		for (uint32_t row = 0; row < size; ++row)
		{
      complex_t* dst_row = dst.get_row(row);
			for (uint32_t col =0; col<size; ++col)
			{
				complex_t accum = zero;
				for (uint32_t i = 0; i < size; ++i)
				{
					accum = add(accum, mul((*this)[row][i], rhs[i][col]));
				}
				dst_row[col] = accum;
			}
		}
#endif // end if opt4, opt3 
	/*
	printf("Setting desination matrix: \n");
	dst.debug_print();
	//*/
}


void ComplexMatrix::mul_herm_for_e_minus_i(const ComplexMatrix& rhs, ComplexMatrix& dst) // changing const of rhs
{
    // TODO: take advantage of the fact that these are diagonally semmetrical
    size_t size = num_rows;
    complex_t zero = to_complex(0.0, 0.0);
    complex_t conj = to_complex(1.0, -1.0);

		/*
		printf("Multiplying this : \n");
		this->debug_print();
		printf("by this : \n");
		rhs.debug_print();
		//*/
		
#if OPT_4
		// printf("OPT 4 \n");
		dst.make_zero();
    for (uint32_t row = 0; row < size; ++row)
    {
			if(this->num_nonzeros_by_row[row] != 0)
			{
				complex_t* dst_row = dst.get_row(row);
//			    const complex_t* src1 = get_row(row);
				for (uint32_t col = row; col<size; ++col)
				{
					if (rhs.num_nonzeros_by_row[col] != 0) 
					{
				  //	const complex_t* src2 = rhs.get_row(col);
						complex_t accum = zero;
			  		
						for (uint32_t j = 0; j < this -> max_nnz_in_a_row; j++)
						{
							uint32_t col_loc = this->nonzero_col_locations[row][j];
							if(col_loc != num_cols)
							{
								for (uint32_t k=0; k < rhs.max_nnz_in_a_row; k++)
								{
									if (col_loc == rhs.nonzero_col_locations[col][k])
									{
										accum = add(accum, mul(this->nonzero_values[row][j], rhs.nonzero_values[col][k]*conj ));					
									}					
								}
							}
						}
						dst_row[col] = accum;
						dst[col][row] = (accum*conj); 

					}
					}
				} // end for (row) loop

		}

#elif OPT_3

		for (uint32_t row = 0; row < size; ++row)
		{
      // Here we make the assumption that the output will be symmetric across
      // the diagonal, so we only need to calculate half of it, and  then
      // write the other half to match.
      complex_t* dst_row = dst.get_row(row);
      const complex_t* src1 = get_row(row);
      for (uint32_t col = row; col < size; ++col)
      {
          const complex_t* src2 = rhs.get_row(col);
          complex_t accum = zero;
          for (uint32_t i = 0; i < size; ++i)
              accum = add(accum, mul(src1[i], src2[i] * conj));
          dst_row[col] = accum;
          dst[col][row] = accum * conj; // This * conj may belong on the previous line
      }
    } // end for (row) loop
#elif mul_full
		for (uint32_t row = 0; row < size; ++row)
		{
      complex_t* dst_row = dst.get_row(row);
			for (uint32_t col =0; col<size; ++col)
			{
				complex_t accum = zero;
				for (uint32_t i = 0; i < size; ++i)
				{
					accum = add(accum, mul((*this)[row][i], rhs[i][col]));
				}
				dst_row[col] = accum;
			}
		}
#endif // end if opt4, opt3 
	/*
	printf("Setting desination matrix: \n");
	dst.debug_print();
	//*/
}




// TODO: Add add_complex_scaled_hermition fnc.... for Cos + i Sin step
// this += rhs * scale
void ComplexMatrix::add_scaled_hermitian(const ComplexMatrix& rhs, const scalar_t& scale)
{
    // TODO: take advantage of the fact that these are diagonally semmetrical
    for (size_t i = 0; i < num_rows * num_rows; ++i)
        values[i] = add(values[i], mul_scalar(rhs.values[i], scale));
}

void ComplexMatrix::add_complex_scaled_hermitian(const ComplexMatrix& rhs, const complex_t& scale)
{
    // TODO: take advantage of the fact that these are diagonally semmetrical
    for (size_t i = 0; i < num_rows * num_rows; ++i)
//        values[i] = add(values[i], mul_complex_scalar(rhs.values[i], scale));
        values[i] = add(values[i], mul(rhs.values[i], scale));
}

void ComplexMatrix::add_hermitian(const ComplexMatrix& rhs)
{
	for(size_t i = 0; i<num_rows*num_cols; i++)
		values[i] = add(values[i], rhs.values[i]);
}


void ComplexMatrix::expm_special(ComplexMatrix& dst, double precision) const
{
    // To avoid extra copying, we alternate power accumulation matrices
    ComplexMatrix power_accumulator0(num_rows, num_cols);
    ComplexMatrix power_accumulator1(num_rows, num_cols);
    power_accumulator0.make_identity();
    power_accumulator1.make_identity();
    ComplexMatrix* pa[2] = {&power_accumulator0, &power_accumulator1};

    dst.make_zero();

    double one_over_k_factorial = 1.0;
    bool done = false;
    for (uint32_t k = 0; !done; ++k)
    {
        if (k > 1)
            one_over_k_factorial /= k;
        if (one_over_k_factorial >= precision)
        {
            uint32_t alternate = k & 1;
            ComplexMatrix& new_pa = *pa[alternate];
            ComplexMatrix& old_pa = *pa[1 - alternate];
            if (k > 0)
            {
                new_pa.compress_matrix_storage();
                old_pa.compress_matrix_storage();

                old_pa.mul_hermitian(*this, new_pa);
						}	
            scalar_t one_over_k_factorial_simd = to_scalar(one_over_k_factorial);
            dst.add_scaled_hermitian(new_pa, one_over_k_factorial_simd);
        }
        else
        {
            done = true;
        }
    }
}

void ComplexMatrix::expm_minus_i_h_t(ComplexMatrix& dst, double time, double precision) const
{
		printf("Time = %lf \n", time); 
		// TODO: add double t argument to this function; make t^k / k!
    // To avoid extra copying, we alternate power accumulation matrices
    bool rescale_method = true;
    double norm_scalar;
    bool do_print = false;
		ComplexMatrix rescaled_mtx(num_rows, num_cols);
    if(rescale_method)
    {
		  norm_scalar = this -> normalise_matrix_by_magnitude();
		  scalar_t scale = to_scalar(1.0/norm_scalar);
			rescaled_mtx.make_zero();
			rescaled_mtx.add_scaled_hermitian(*this, scale);

		  printf("After rescaling by factor %lf: \n", norm_scalar);
			printf("Ratio of time to rescale factor = %15f \n", time/norm_scalar);
			printf("Product of time to rescale factor = %15f \n", time*norm_scalar);
			rescaled_mtx.debug_print();
			rescaled_mtx.compress_matrix_storage();
		  //this -> debug_print();
		  //this -> compress_matrix_storage();
		}
    ComplexMatrix power_accumulator0(num_rows, num_cols);
    ComplexMatrix power_accumulator1(num_rows, num_cols);
    power_accumulator0.make_identity();
    power_accumulator1.make_identity();
    ComplexMatrix* pa[2] = {&power_accumulator0, &power_accumulator1};

    dst.make_zero();

    double one_over_k_factorial = 1.0;
    double time_tracker = 1.0;
    double k_fact = 1.0;
    double scale_tracker = 1.0;
		complex_t current_max_element = this -> get_max_mtx_element();
    
//		one_over_k_factorial *= time;
    bool done = false;
    for (uint32_t k = 0; !done; ++k)
//    for (uint32_t k = 0; k<5; ++k)
    {
        if (k > 0)
      	{
						one_over_k_factorial *= time;
        		one_over_k_factorial /= k;
						time_tracker *= time;
						k_fact /= k;
						//printf(" k=%u. \t 1/k! = %.3e \n", k, k_fact);
						if (rescale_method) 
						{
							one_over_k_factorial *= norm_scalar;
							scale_tracker *= norm_scalar;
        		}
				}
				

        if (one_over_k_factorial * ::mag_sqr(current_max_element) >= precision)
//        if (one_over_k_factorial >= precision)
        {
            uint32_t alternate = k & 1;
            ComplexMatrix& new_pa = *pa[alternate];
            ComplexMatrix& old_pa = *pa[1 - alternate];
            if (k > 0)
            {
                new_pa.compress_matrix_storage();
                old_pa.compress_matrix_storage();

								if(do_print)
								{
									printf("Old_pa: \n");
									old_pa.debug_print();
									printf("New pa: \n");
									new_pa.debug_print();
									printf("this: \n");
									this -> debug_print();
								}
									
//                old_pa.mul_herm_for_e_minus_i(*this, new_pa);
                old_pa.mul_herm_for_e_minus_i(rescaled_mtx, new_pa);                
                
                //new_pa.debug_print();
                current_max_element = new_pa.get_max_mtx_element();
								//printf("k=%u. Max_el * scale = %.5e \n",k, ::mag_sqr(current_max_element) * one_over_k_factorial);
								//printf("current max = %.5e + %.5e i \n", get_real(current_max_element), get_imag(current_max_element));
						}	
						
            complex_t one_over_k_factorial_simd;
            
            						/* Set symmetrical element */
						if( (k)%4 == 0 )
						{
							one_over_k_factorial_simd = to_complex(one_over_k_factorial, 0.0); 
						}
						else if ((k+1)%4 == 0 )
						{
							one_over_k_factorial_simd = to_complex(0.0, 1.0*one_over_k_factorial); 
						}					
						else if ((k+2)%4 == 0)
						{
							one_over_k_factorial_simd = to_complex(-1.0*one_over_k_factorial, 0.0); 
						}
						else if ( (k+3)%4 ==0)
						{
							one_over_k_factorial_simd = to_complex(0.0, -1.0*one_over_k_factorial); 
						}
						else 
						{
							printf("k = %u doesn't meet criteria.\n", k);
						}

						
//            if(!std::isfinite(scale_tracker*time_tracker*k_fact) || (k_fact < precision))
            if(!std::isfinite(scale_tracker*time_tracker*k_fact) || (scale_tracker*time_tracker*k_fact < precision) || k_fact < 1e-300)
            {
            	if(!std::isfinite(scale_tracker*time_tracker*k_fact)) 
            	{
            		printf("Value is not finite at k=%u. \n", k);
            	}
            	printf("Time^k = %.3e \n", time_tracker);
            	printf("Scale^k = %.3e \n", scale_tracker);
            	printf("(Time * Scale)^k = %.3e \n", time_tracker * scale_tracker);
            	printf("1/k! = %.3e \n", k_fact); 
							printf("Value = %.3e \n", scale_tracker*time_tracker*k_fact);
            	
							printf("Exponentiation expansion truncated at k=%u \n", k);
            	done = true;
            }
            else
            { //only add to destination matrix if not yet at inf
		          dst.add_complex_scaled_hermitian(new_pa, one_over_k_factorial_simd);

							/* Print this loop */
							if(do_print)
							{
								printf("k=%u \n \t t^k = %.5e \n \t norm^k = %.5e \n \t 1/k! = %.5e \n", k, time_tracker, scale_tracker, k_fact ); 
								printf("Scaling factor: %.5e + %.5e i \n", get_real(one_over_k_factorial_simd), get_imag(one_over_k_factorial_simd));
								printf("Adding : \n");
								new_pa.debug_print();
				        printf("Then dest: \n");
				        dst.debug_print();
							}
            
            }
            
        }
        else
        {
            done = true;
						printf("Exponentiation expansion truncated at k=%u \n", k);
						printf("Here (s.t)^k/k! = %.5e \n", one_over_k_factorial);
						printf("current max = %.5e + %.5e i \n", get_real(current_max_element), get_imag(current_max_element));						
        }
    }

    /*
    if (rescale_method) 
    {
    	this -> restore_norm(norm_scalar);
			this -> compress_matrix_storage();
		}*/
}



void ComplexMatrix::cos_plus_i_sin(ComplexMatrix& dst, double precision) const
{
    // To avoid extra copying, we alternate power accumulation matrices
    ComplexMatrix power_accumulator0(num_rows, num_cols);
    ComplexMatrix power_accumulator1(num_rows, num_cols);
    ComplexMatrix sin(num_rows, num_cols);
    ComplexMatrix cos(num_rows, num_cols);
    power_accumulator0.make_identity();
    power_accumulator1.make_identity();
		ComplexMatrix* pa[2] = {&power_accumulator0, &power_accumulator1};

		ComplexMatrix empty_matrix_0(num_rows, num_cols);
		ComplexMatrix empty_matrix_1(num_rows, num_cols);
		empty_matrix_0.make_identity();
		empty_matrix_1.make_identity();
		ComplexMatrix* empty[2] = {&empty_matrix_0, &empty_matrix_1};
				
		ComplexMatrix identity(num_rows, num_cols);
		identity.make_identity();

		ComplexMatrix& hamiltonian = *empty[0];
		ComplexMatrix& h_squared = *empty[1];
		
		identity.compress_matrix_storage();
		identity.mul_hermitian(*this, hamiltonian);
		hamiltonian.compress_matrix_storage();
		hamiltonian.mul_hermitian(*this, h_squared);

		h_squared.compress_matrix_storage();

    dst.make_zero();
		sin.make_zero();
		cos.make_identity();
		
    double one_over_k_factorial = 1.0;
    bool done = false;
    // work out cos(H)
    /*
		printf("Hamiltonian: \n");
		hamiltonian.debug_print();
		printf("Hamiltonian Squared: \n");
		h_squared.debug_print();
			
		printf("Then, Cos = \n");
		cos.debug_print();
		//*/
		uint32_t alternate;	
		double plus_or_minus = 1.0;
//    for (uint32_t k = 2; k <= 6; k+=2)
    for (uint32_t k = 2; !done; k+=2)
    {
        if (k > 1)
        {
          one_over_k_factorial /= (k*(k-1)); // need to account for skipped k 
					if(k%4 != 0)
					{
						plus_or_minus = -1.0;
						alternate = 0;
					}
					else
					{
						plus_or_minus = 1.0;
						alternate = 1;
					}	
					// TODO: figure out when powers of k should be negative. Note cos goes like H^2k and (-1)^k

		      if (one_over_k_factorial >= precision)
		      {
//	          uint32_t alternate = k & 1;
	          ComplexMatrix& new_pa = *pa[alternate];
	          ComplexMatrix& old_pa = *pa[1 - alternate];

						/*
						printf("[cos] (old_pa) : \n");
						old_pa.debug_print();
						printf("[cos] (new_pa) : \n");
						new_pa.debug_print();
						//*/
						//new_pa.compress_matrix_storage();

            old_pa.compress_matrix_storage();
            old_pa.mul_hermitian(h_squared, new_pa);
						

	          scalar_t one_over_k_factorial_simd = to_scalar(one_over_k_factorial*plus_or_minus);
	          cos.add_scaled_hermitian(new_pa, one_over_k_factorial_simd);
						/*
	         	printf("[Cos] k=%u \t +/- = %f \t 1/k! = %.52f \n", k, plus_or_minus, one_over_k_factorial);
	         	printf("Adding: \n");
	         	new_pa.debug_print(); 
	         	printf("Then, Cos = \n");
	          cos.debug_print();
						//*/

		      }
		      else
		      {
		          done = true;
		          printf("Precision %lf reached at k = %u \n", precision, k);
		      }
		      
				}
    }

		// Work out sin(H)
		pa[0] = &hamiltonian;
		pa[1] = &hamiltonian;
 		hamiltonian.compress_matrix_storage();
		done = false;
    one_over_k_factorial = 1.0;
		plus_or_minus = 1.0;

		sin.add_hermitian(hamiltonian);

    for (uint32_t k = 3; !done; k+=2)
//    for (uint32_t k = 3; k<6; k+=2)
    {
        if (k >= 1)
        {
					if((k+1)%4 == 0)
					{
						plus_or_minus = -1.0;
					}
					else
					{
						plus_or_minus = 1.0;
					}	

          if(k>1) one_over_k_factorial /= (k*(k-1)); // need to account for skipped k 
		      if (one_over_k_factorial >= precision)
		      {
		          uint32_t alternate = k & 1;
		          ComplexMatrix& new_pa = *pa[alternate];
		          ComplexMatrix& old_pa = *pa[1 - alternate];
							/*
							printf("[sin] (old_pa) : \n");
							old_pa.debug_print();
							printf("[sin] (new_pa) : \n");
							new_pa.debug_print();
							//*/
							//new_pa.compress_matrix_storage();
              old_pa.compress_matrix_storage();
              // printf("To be multiplied by h_squared : \n");
              // old_pa.debug_print();
              // printf("H squared: \n");
              //h_squared.debug_print();
              old_pa.mul_hermitian(h_squared, new_pa);



		          scalar_t one_over_k_factorial_simd = to_scalar(one_over_k_factorial*plus_or_minus);
		          sin.add_scaled_hermitian(new_pa, one_over_k_factorial_simd);
		         	//*
		         	printf("[Sin] k=%u \t +/- = %f \t 1/k! = %.52f \n", k, plus_or_minus, one_over_k_factorial);
		         	//printf("Adding: \n");
		         	//new_pa.debug_print(); 
		         	printf("Then, Sin = \n");
		         	sin.debug_print();
		         	//*/
		      }
		      
		      
		      else
		      {
		          done = true;
		          // printf("Precision %.52f reached at k= %u \n", precision, k);
		      }
				}
    }
		/*
		printf("Cos =: \n");
		cos.debug_print();
		printf("Sin =: \n"); 
		sin.debug_print();
		//*/
//*
		// TODO: fnc to swap elements, i..e multiply by i
		for(uint32_t r =0; r<num_rows; r++)
		{
			for(uint32_t c = 0; c<num_cols; c++)
			{
				sin[r][c] = to_complex(-1.0*get_imag(sin[r][c]), get_real(sin[r][c]));
			}
		}

		dst.add_hermitian(cos);
		dst.add_hermitian(sin);
		/*
		printf("---- Exponentiated ---- \n");
		dst.debug_print();
		//*/
//*/
		
}




/* Adapted from Brian Butler's Matlab implementation here:
 *  https://www.mathworks.com/matlabcentral/fileexchange/
 *   53784-matrix-permanent-using-nijenhuis-wilf-in-cmex/content/perman_mat.m
 */

#define MAX_PERM_SIZE 128

complex_t do_permanent(const complex_t* mtx_data, uint32_t size)
{
    if (size > MAX_PERM_SIZE)
    {
        printf("Error: MAX_PERM_SIZE needs to be increased.\n");
        return to_complex(0, 0);
    }
    complex_t xrow_storage[MAX_PERM_SIZE];

    complex_t* xrow = xrow_storage;
    scalar_t half = to_scalar(0.5);
    complex_t p = to_complex(1.0, 0.0);

    for (size_t i = 0; i < size; ++i)
    {
        complex_t sum = to_complex(0.0, 0.0);
        for (size_t j = 0; j < size; ++j)
        {
            addeq(&sum, load_complex(&mtx_data[j * size + i]));
        }
        xrow[i] = sub(load_complex(&mtx_data[(size - 1) * size + i]), mul_scalar(sum, half));
        muleq(&p, xrow[i]);
    }

    uint64_t tn11 = (1L << (size - 1)) - 1;
    uint64_t y_prev = 0;
    for (uint64_t i = 0; i < tn11; ++i)
    {
        uint64_t yi = (i+1) ^ ((i+1) >> 1);
        uint32_t zi = int_log2(yi ^ y_prev);
        scalar_t si = to_scalar(-1.0 + 2.0 * bit_get(yi, zi));

        y_prev = yi;

        complex_t prodx = to_complex(1.0, 0.0);
        uint32_t offset = zi * size;
        for (uint32_t j = 0; j < size; ++j)
        {
            complex_t rr = add(xrow[j], mul_scalar(load_complex(&mtx_data[offset + j]), si));
            xrow[j] = rr;
            muleq(&prodx, rr);
        }
        if (i & 1)
            addeq(&p, prodx);
        else
            subeq(&p, prodx);
    }

    addeq(&p, p);
    if (!(size & 1))
        muleq_scalar(&p, to_scalar(-1.0));
    return p;
}

double do_permanent(const double* mtx_data, uint32_t size)
{
    if (size > MAX_PERM_SIZE)
    {
        printf("Error: MAX_PERM_SIZE needs to be increased.\n");
        return 0;
    }
    double xrow_storage[MAX_PERM_SIZE];

    double* xrow = xrow_storage;
    double p = 1.0;

    for (size_t i = 0; i < size; ++i)
    {
        double sum = 0.0;
        for (size_t j = 0; j < size; ++j)
            sum += mtx_data[j * size + i];
        xrow[i] = mtx_data[(size - 1) * size + i] - sum * 0.5;
        p *= xrow[i];
    }

    uint64_t tn11 = (1L << (size - 1)) - 1;
    uint64_t y_prev = 0;
    for (uint64_t i = 0; i < tn11; ++i)
    {
        uint64_t yi = (i+1) ^ ((i+1) >> 1);
        uint32_t zi = int_log2(yi ^ y_prev);
        double si = -1.0 + 2.0 * bit_get(yi, zi);

        y_prev = yi;

        double prodx = 1.0;
        uint32_t offset = zi * size;
        for (uint32_t j = 0; j < size; ++j)
        {
            double rr = xrow[j] + mtx_data[offset + j] * si;
            xrow[j] = rr;
            prodx *= rr;
        }
        if (i & 1)
            p += prodx;
        else
            p -= prodx;
    }

    p += p;
    if (!(size & 1))
        p = -p;
    return p;
}

void ComplexMatrix::debug_print() const
{
//		printf("---- -- Debug Print -- ----\n");

    for (uint32_t row = 0; row < num_rows; ++row)
    {
        const complex_t* prow = get_row(row);
        printf("  | ");
        for (uint32_t col = 0; col < num_cols; ++col)
        {
            complex_t val = prow[col];
            double re = get_real(val);
            double im = get_imag(val);
            if (re || im)
                printf("%.7e+%.7ei ", get_real(val), get_imag(val));
            else
                printf("     0     ");
        }
        printf(" |\n");
    }
	printf("---- ---- ----\n");
}
