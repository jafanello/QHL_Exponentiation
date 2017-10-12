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
#define PRINT_LINE_DEBUG 0
#define SPARSE_METHOD 0
#define PRINT_MATRICES_MULT 1


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
	printf("[print compressed] Max nnz in any row : %u \n", max_nnz_in_a_row);

	for(uint32_t i=0; i<num_rows; i++){
		printf("[print compressed] There are %u Nonzeros in row %u :\n",num_nonzeros_by_row[i], i);
		for(uint32_t j=0; j<num_nonzeros_by_row[i]; j++)
		{
		  printf("\t [print compressed] (i,j) = (%u, %u)", i, j);
			printf("\t Loc :%u, \t", nonzero_col_locations[i][j]);
			complex_t val = nonzero_values[i][j];
			printf("Val :\t  %.4e+%.4ei , \n", get_real(val), get_imag(val));
		}
	}
  printf("[print compressed] End of print\n");
}

#define OPT_3 0 // opt 3 correctly exploits Symmetrical shape, but not sparsity. 
#define OPT_4 1 // opt 4 used for development of sparsity utility
#define mul_full 0


// dst = this * rhs
void ComplexMatrix::mul_hermitian(const ComplexMatrix& rhs, ComplexMatrix& dst) // changing const of rhs
{
    size_t size = num_rows;
    complex_t zero = to_complex(0.0, 0.0);
    complex_t conj = to_complex(1.0, -1.0);
		
#if OPT_4
		// printf("OPT 4 \n");
		dst.make_zero();
    for (uint32_t row = 0; row < size; ++row)
    {
			if(this->num_nonzeros_by_row[row] != 0)
			{
				complex_t* dst_row = dst.get_row(row);
				for (uint32_t col = row; col<size; ++col)
				{
					if (rhs.num_nonzeros_by_row[col] != 0) 
					{
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
}


void ComplexMatrix::sparse_hermitian_mult(const ComplexMatrix& rhs, ComplexMatrix& dst)
{
    if(PRINT_MATRICES_MULT)
    {
      printf("[Sparse Mult] LHS: \n");
      this->print_compressed_storage();
      printf("[Sparse Mult] RHS: \n");
      rhs.print_compressed_storage();
    }
     
    // Multiplication of one sparse Hermitian matrix by another. 
    size_t size = this->num_rows;
    complex_t zero = to_complex(0.0, 0.0);
    complex_t conj = to_complex(1.0, -1.0);
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
    uint32_t num_rows = this->num_rows;
    printf("[mult] num rows = %u \n", num_rows);
		//dst.make_zero();
    uint32_t num_elements = num_rows*num_rows;
		complex_t nonzero_values[num_elements];
    uint32_t rows[num_elements];
    uint32_t cols[num_elements];
    uint32_t row_full_upto[num_rows];
    uint32_t k = 0;
    uint32_t total_num_nnz = 0;    
  
    uint32_t temp_nnz_by_row[num_rows];
    for(uint32_t i=0; i<num_rows; i++)
    {
      temp_nnz_by_row[i] = 0;
    }
    
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    
    for (uint32_t row = 0; row < size; ++row)
    {
			if(this->num_nonzeros_by_row[row] != 0)
			{
//				complex_t* dst_row = dst.get_row(row);
				for (uint32_t col = row; col<size; ++col)
				{
					if (rhs.num_nonzeros_by_row[col] != 0) 
					{
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

            if(get_real(accum)!=0.0 || get_imag(accum)!=0.0)
            {
              printf("Row %u Col %u Val %.2e + %.2e i \n", row, col, get_real(accum), get_imag(accum));
              nonzero_values[k] = accum;
              rows[k] = row;
              cols[k] = col;
              temp_nnz_by_row[row]++;

              k++;
              total_num_nnz ++;

              if(row!=col)
              {
              printf("Row %u Col %u Val %.2e + %.2e i \n", col, row, get_real(accum), get_imag(accum*conj));
                nonzero_values[k] = accum * conj;
                rows[k] = col;
                cols[k] = row;  
                k++;
                total_num_nnz ++;
                temp_nnz_by_row[col]++;
              }
              
            }
            
//						dst_row[col] = accum;
//						dst[col][row] = (accum*conj); 

					}
				}
			}
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
			 
	  } // end for (row) loop

    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    uint32_t max_nnz = 0;
    for(uint32_t a=0; a<num_rows; a++)
    {
      if(temp_nnz_by_row[a] > max_nnz)
      {
        max_nnz = temp_nnz_by_row[a];
      }
    }
    printf("Max NNZ = %u \n", max_nnz);
    for(uint32_t i=0; i<num_rows;i++)
    {
      row_full_upto[i] = 0;
    }
  
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);


    uint32_t* tmp_nnz_by_row;
    tmp_nnz_by_row = new uint32_t[num_rows];
    
    complex_t** tmp_nnz_vals;
    tmp_nnz_vals = new complex_t*[num_rows];
    uint32_t** tmp_nnz_col_locs;
    tmp_nnz_col_locs = new uint32_t*[num_rows];
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
    
    for(uint32_t i=0; i<num_rows; i++)
    {
      tmp_nnz_vals[i] = new complex_t[max_nnz];
      tmp_nnz_col_locs[i] = new uint32_t[max_nnz];
    }
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
    
    uint32_t a = 0;
    for(uint32_t k=0; k<total_num_nnz; k++)
    {
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
    
      uint32_t row = rows[k];
      uint32_t col = cols[k];
      complex_t val = nonzero_values[k];
      a = row_full_upto[row];
      
      tmp_nnz_vals[row][a] = val;
      tmp_nnz_col_locs[row][a] = col;
      row_full_upto[row]++;
    }
    /*
    for(uint32_t i=0; i<num_rows; i++)
    {
      for(uint32_t j=tmp_nnz_by_row[i]; j< 
    }
    */
    uint32_t tmp_max_nnz = 0;
    for(uint32_t i=0; i<num_rows; i++)
    {
      tmp_nnz_by_row[i] = temp_nnz_by_row[i];
      if(tmp_nnz_by_row[i] > tmp_max_nnz)
      {
        tmp_max_nnz = tmp_nnz_by_row[i];
      }
    }

    printf("[mult - end] tmp_max_nnz = %u \nTmp Values: \n", tmp_max_nnz);
    for(uint32_t i=0; i<num_rows; i++)
    {     
      printf("[mult - end] Row %u has %u nnz vals \n", i, tmp_nnz_by_row[i]);
      for(uint32_t j=0; j<tmp_max_nnz; j++)
      {
        complex_t val = tmp_nnz_vals[i][j];
        uint32_t col = tmp_nnz_col_locs[i][j];
        printf("[mult - end] Col loc: %u \t val: %.2e+%.2ei \n", col, get_real(val), get_imag(val));
      }
      
    }

    printf("[mult - end] New max_nnz going into reallocate fnc = %u \n", tmp_max_nnz);

    dst.reallocate(tmp_max_nnz, tmp_nnz_by_row, tmp_nnz_vals, tmp_nnz_col_locs);    

    if(PRINT_MATRICES_MULT)
    {
      printf("[Sparse Mult] Result of MULT : \n");
      dst.print_compressed_storage();
    }
}





void ComplexMatrix::mul_herm_for_e_minus_i(const ComplexMatrix& rhs, ComplexMatrix& dst) // changing const of rhs
{
    // TODO: take advantage of the fact that these are diagonally semmetrical
    size_t size = num_rows;
    complex_t zero = to_complex(0.0, 0.0);
    complex_t conj = to_complex(1.0, -1.0);

#if OPT_4
		dst.make_zero();
    for (uint32_t row = 0; row < size; ++row)
    {
			if(this->num_nonzeros_by_row[row] != 0)
			{
				complex_t* dst_row = dst.get_row(row);
				for (uint32_t col = row; col<size; ++col)
				{
					if (rhs.num_nonzeros_by_row[col] != 0) 
					{
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
            // instead of filling here, fill nnz_array ??
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
}


// this += rhs * scale
void ComplexMatrix::add_scaled_hermitian(const ComplexMatrix& rhs, const scalar_t& scale)
{
    for (size_t i = 0; i < num_rows * num_rows; ++i)
        values[i] = add(values[i], mul_scalar(rhs.values[i], scale));
}

void ComplexMatrix::add_complex_scaled_hermitian(const ComplexMatrix& rhs, const complex_t& scale)
{
    for (size_t i = 0; i < num_rows * num_rows; ++i)
        values[i] = add(values[i], mul(rhs.values[i], scale));
}


void ComplexMatrix::add_complex_scaled_hermitian_sparse(const ComplexMatrix& rhs, const complex_t& scale)
{
    printf("[addition fnc] THIS: \n");
    this -> print_compressed_storage();
    printf("[addition fnc] RHS: \n");
    rhs.print_compressed_storage();
    /* Set up arrays/pointers etc. to send to reallocate function at end. */
    uint32_t num_rows = this->num_rows;
    printf("Num rows: %u \n", num_rows);
    uint32_t upper_max_nnz = this->max_nnz_in_a_row + rhs.max_nnz_in_a_row;
    
    uint32_t new_max_nnz=0;
    uint32_t* tmp_nnz_by_row;
    tmp_nnz_by_row = new uint32_t[num_rows];
    
    complex_t** tmp_nnz_vals;
    tmp_nnz_vals = new complex_t*[num_rows];
    uint32_t** tmp_nnz_col_locs;
    tmp_nnz_col_locs = new uint32_t*[num_rows];
    
    for(uint32_t i=0; i<num_rows; i++)
    {
      tmp_nnz_vals[i] = new complex_t[upper_max_nnz];
      tmp_nnz_col_locs[i] = new uint32_t[upper_max_nnz];
      tmp_nnz_by_row[i] = 0;
    }

    /* Actual addition */
    
    for(uint32_t row=0; row<num_rows; row++)
    {
      uint32_t a;
      uint32_t b;
      uint32_t c;
      uint32_t rhs_unused_cols[num_rows*num_rows];
      uint32_t this_unused_cols[num_rows*num_rows];
      uint32_t this_row_num_nnz = this -> num_nonzeros_by_row[row];
      uint32_t rhs_row_num_nnz = rhs.num_nonzeros_by_row[row];
      uint32_t row_nnz_counter = 0;
      printf("[addition fnc] Row %u: this_nnz: %u \t rhs_nnz: %u \n", row, this_row_num_nnz, rhs_row_num_nnz);
      a=b=c=0;
      printf("[addition fnc] Before row loop: a=%u \t b=%u \t c=%u \n", a,b,c);
      uint32_t rhs_cols_been_added[rhs_row_num_nnz];
      for(uint32_t i=0; i<rhs_row_num_nnz; i++)
      {
        rhs_cols_been_added[i] = 0;
      }
      
      if (this_row_num_nnz == 0 )
      {
        printf("Row %u has 0 entries (%u) in LHS and %u in RHS \n", row, this->num_nonzeros_by_row[row], rhs_row_num_nnz);
        for(uint32_t j=0; j<rhs_row_num_nnz; j++)
        {
            rhs_unused_cols[b] = j;
            b++;
        }
      }
      else
      {
        for(uint32_t i=0; i<this_row_num_nnz; i++)
        {
          uint32_t this_col = this->nonzero_col_locations[row][i];
          bool this_col_matched = 0;
          
          for(uint32_t j=0; j<rhs_row_num_nnz; j++)
          {
            uint32_t rhs_col = rhs.nonzero_col_locations[row][j];
            printf("[addition fnc] row %u \t this col = %u rhs col = %u \n", row, this_col, rhs_col);
            if(this_col == rhs_col)
            {
              tmp_nnz_vals[row][a] = this->nonzero_values[row][i] + mul(rhs.nonzero_values[row][j], scale);
              tmp_nnz_col_locs[row][a] = this_col;
              a++;
              printf("== a++ \n");
              this_col_matched = 1;
              printf("row %u col %u matched \n",row, this_col);
              printf("THIS : %.2e + %.2e i \n", get_real(this->nonzero_values[row][i]), get_imag(this->nonzero_values[row][i]));
              printf("RHS : %.2e + %.2e i \n", get_real(rhs.nonzero_values[row][j]), get_imag(rhs.nonzero_values[row][j]));
              row_nnz_counter++;
              tmp_nnz_by_row[row]++;
              rhs_cols_been_added[j] = 1;
            }
            else
            {
              // has this been used already?
              if(rhs_cols_been_added[j] == 0)
              {
                rhs_unused_cols[b] = j;
                rhs_cols_been_added[j] = 1;
                printf("row %u, RHS col %u unmatched (this col %u)\n", row, rhs_col, this_col);
                b++;
              }
            }
            
          }
          if(this_col_matched == 0)
          {
            this_unused_cols[c] = i;
            printf("row %u, this col %u unmatched\n", row, this_col);
            c++;
          }
        }
      }    

      printf("[addition fnc] Before unused arrays, a=%u b=%u c=%u \n", a,b,c);
      for(uint32_t k=0; k<b; k++)
      {
        uint32_t col = rhs_unused_cols[k];
        printf("[addition fnc] row %u unused RHS col %u \n", row, rhs.nonzero_col_locations[row][col]);
        tmp_nnz_vals[row][a] = mul(rhs.nonzero_values[row][col], scale);
        tmp_nnz_col_locs[row][a] = rhs.nonzero_col_locations[row][col];
        a++;
        printf("k<b a++ \n");
        row_nnz_counter++;
        tmp_nnz_by_row[row]++;

      } 

      for(uint32_t k=0; k<c; k++)
      {
        uint32_t col = this_unused_cols[k];
        printf("[addition fnc] row %u unused this col %u \n", row, this->nonzero_col_locations[row][col]);
        tmp_nnz_vals[row][a] = this->nonzero_values[row][col];
        tmp_nnz_col_locs[row][a] = this->nonzero_col_locations[row][col];
        a++;
        printf("k<c a++ \n");
        row_nnz_counter++;
        tmp_nnz_by_row[row]++;
      } 
      printf("[addition fnc] After Used/Unused: a=%u \t b=%u \t c=%u \n", a,b,c);
      printf("[addition fnc] Row %u Counter %u \n", row, row_nnz_counter);
    }
    
    new_max_nnz = 0;
    for(uint32_t i=0; i<num_rows;i++)
    {
      printf("[tmp_nnz_by_row] i=%u \t nnz= %u \n", i, tmp_nnz_by_row[i]);
      if(tmp_nnz_by_row[i] > new_max_nnz)
      {
        new_max_nnz = tmp_nnz_by_row[i];
      }
    }
    
    printf("[addition fnc] New max_nnz going into reallocate fnc = %u \n", new_max_nnz);
    this->reallocate(new_max_nnz, tmp_nnz_by_row, tmp_nnz_vals, tmp_nnz_col_locs);
    printf("[addition fnc] After addition: \n");
    this-> print_compressed_storage();
}


void ComplexMatrix::add_hermitian(const ComplexMatrix& rhs)
{
	for(size_t i = 0; i<num_rows*num_cols; i++)
		values[i] = add(values[i], rhs.values[i]);
}


bool ComplexMatrix::exp_ham_sparse(ComplexMatrix& dst, double scale, double precision, bool plus_minus) const
{
    /* To avoid extra copying, we alternate power accumulation matrices */
    if(PRINT_LINE_DEBUG) printf("In exp ham: \n");
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    double scalar_by_time = scale;
		bool infinite_val = false; // If the matrix multiplication doesn't diverge, this is set to true and returned to indicate the method has failed. 
    bool rescale_method = true; // Flag to rescale Hamiltonian so that all elements <=1
    double norm_scalar;
    bool do_print = false;

    printf("Num rows: %u cols: %u \n", num_rows, num_cols);
    ComplexMatrix power_accumulator0(num_rows, num_cols);
    ComplexMatrix power_accumulator1(num_rows, num_cols);
    power_accumulator0.make_identity();
    power_accumulator1.make_identity();
    ComplexMatrix* pa[2] = {&power_accumulator0, &power_accumulator1};



    dst.make_zero();
    dst.compress_matrix_storage();
    printf("At beginning: dst \n");
    dst.print_compressed_storage();

    printf("pa[0]: \n");
    pa[0]->debug_print();
    printf("pa[1]: \n");
    pa[1]->debug_print();

    double k_fact = 1.0;
		double scale_time_over_k_factorial = 1.0;
		// double current_max_element = this -> get_max_element_magnitude();
    bool done = false;
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    for (uint32_t k = 0; !done; ++k)
    {
        if (k > 0)
      	{
						k_fact /= k;
						scale_time_over_k_factorial *= scalar_by_time/k;
				}
				
//        if (scale_time_over_k_factorial * current_max_element >= precision)
        if (scale_time_over_k_factorial >= precision)
        { 
        /* 
        * This is where actual exponentiation happens by multiplying a running total,
        * H^m, by H to get H^m. 
        * This is then multiplied by (t*s)^m/m! and added to become the new running total.
        * Here s is a scalar - the largest magnitude of any matrix element. This is factored out 
        * of the matrix so that all values inside the matrix are less than one, 
        * to keep the multiplication from diverging and introducing matrix elements 
        * larger than the computer can handle.
        * t is the time set by the heuristic. 
        *
        */
        
            uint32_t alternate = k & 1;
            ComplexMatrix& new_pa = *pa[alternate];
            ComplexMatrix& old_pa = *pa[1 - alternate];
            printf("Inside k loop, new_pa: \n");
            new_pa.debug_print();

            new_pa.compress_matrix_storage();
            old_pa.compress_matrix_storage();
            printf("Compressed before exp \n");
            printf("new_pa:\n");
            new_pa.print_compressed_storage();
            printf("old_pa:\n");
            old_pa.print_compressed_storage();


            if (k > 0)
            {
                //new_pa.compress_matrix_storage();
                //old_pa.compress_matrix_storage();
								//new_pa.make_zero();
								printf("[exp mult k=%u ]  \n", k);
								printf("[exp mult k=%u ] old_pa: \n", k);
								old_pa.print_compressed_storage();
								printf("[exp mult k=%u ] by THIS: \n", k);
								this->print_compressed_storage();
                
                old_pa.sparse_hermitian_mult(*this, new_pa);                
                //old_pa.mul_herm_for_e_minus_i(*this, new_pa);                

								printf("[exp mult k=%u] After: new_pa: \n", k);
								new_pa.print_compressed_storage();	

						}	
						
            complex_t one_over_k_factorial_simd;
            
            /* Set symmetrical element */
            //printf("plus_minus = %u\n", plus_minus);
            if(plus_minus == true) // plus_minus = true -> (+i) 
            {
						  if((k)%4 == 0 ) // k=0,4,8...
						  {
							  one_over_k_factorial_simd = to_complex(scale_time_over_k_factorial, 0.0); 
						  }
						  else if ( (k+3)%4 ==0) // k = 1,5,9
						  {
							  one_over_k_factorial_simd = to_complex(0.0, 1.0*scale_time_over_k_factorial); 
						  }
						  else if ((k+2)%4 == 0) // k = 2,6,10
						  {
							  one_over_k_factorial_simd = to_complex(-1.0*scale_time_over_k_factorial, 0.0); 
						  }
						  else if ((k+1)%4 == 0 ) // k =3, 7, 11
						  {
							  one_over_k_factorial_simd = to_complex(0.0, -1.0*scale_time_over_k_factorial); 
						  }					
            }
            else
            { // plus_minus = false -> (-i)
						  if((k)%4 == 0 ) // k=0,4,8...
						  {
							  one_over_k_factorial_simd = to_complex(scale_time_over_k_factorial, 0.0); 
						  }
						  else if ( (k+3)%4 ==0) // k = 1,5,9
						  {
							  one_over_k_factorial_simd = to_complex(0.0, -1.0*scale_time_over_k_factorial); 
						  }
						  else if ((k+2)%4 == 0) // k = 2,6,10
						  {
							  one_over_k_factorial_simd = to_complex(-1.0*scale_time_over_k_factorial, 0.0); 
						  }
						  else if ((k+1)%4 == 0 ) // k =3, 7, 11
						  {
							  one_over_k_factorial_simd = to_complex(0.0, 1.0*scale_time_over_k_factorial); 
						  }					
						  else 
						  {
							  printf("k = %u doesn't meet criteria.\n", k);
						  }
            }
    
            if(!std::isfinite(scale_time_over_k_factorial) || k_fact < 1e-300)
            {
            /* 
            * If values are intractable using double floating point precision,
            * fail the process and the function returns 1 to indicate failure.
            */
            	done = true;
            	infinite_val = true;
            }
            //else if (scale_time_over_k_factorial < precision)
            else if (scale_time_over_k_factorial < precision)
            {
            	done = true;
            }
            
            else
            { /* only add to destination matrix if not yet at inf */
              printf("[exp addition k= %u]  adding new_pa by scalar %.2e + %.2e: \n",k, get_real(one_over_k_factorial_simd), get_imag(one_over_k_factorial_simd));
              new_pa.print_compressed_storage();
              printf("[exp addition k= %u] adding to dst: \n",k);
              dst.print_compressed_storage();
		          dst.add_complex_scaled_hermitian_sparse(new_pa, one_over_k_factorial_simd);
              printf("[exp addition k= %u] After addition, dst: \n",k);
              dst.print_compressed_storage();
            }
            
        }
        else
        {
            done = true;
        }
    }
    //dst.compress_matrix_storage();
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);
    printf("\n \n \nEND OF EXP: RESULT \n");
    dst.print_compressed_storage();
    return infinite_val;
}

// TODO separate fnc: sparse_exp_ham
bool ComplexMatrix::exp_ham(ComplexMatrix& dst, double scale, double precision, bool plus_minus) const
{
    /* To avoid extra copying, we alternate power accumulation matrices */
    if(PRINT_LINE_DEBUG) printf("In exp ham: \n");
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    double scalar_by_time = scale;
		bool infinite_val = false; // If the matrix multiplication doesn't diverge, this is set to true and returned to indicate the method has failed. 
    double norm_scalar;
    bool do_print = false;

    printf("Num rows: %u cols: %u \n", num_rows, num_cols);
    ComplexMatrix power_accumulator0(num_rows, num_cols);
    ComplexMatrix power_accumulator1(num_rows, num_cols);
    power_accumulator0.make_identity();
    power_accumulator1.make_identity();
    ComplexMatrix* pa[2] = {&power_accumulator0, &power_accumulator1};

    dst.make_zero();

    double k_fact = 1.0;
		double scale_time_over_k_factorial = 1.0;
		// double current_max_element = this -> get_max_element_magnitude();
    bool done = false;
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    for (uint32_t k = 0; !done; ++k)
    {
        if (k > 0)
      	{
						k_fact /= k;
						scale_time_over_k_factorial *= scalar_by_time/k;
				}
				
        if (scale_time_over_k_factorial >= precision)
        { 
        /* 
        * This is where actual exponentiation happens by multiplying a running total,
        * H^m, by H to get H^m. 
        * This is then multiplied by (t*s)^m/m! and added to become the new running total.
        * Here s is a scalar - the largest magnitude of any matrix element. This is factored out 
        * of the matrix so that all values inside the matrix are less than one, 
        * to keep the multiplication from diverging and introducing matrix elements 
        * larger than the computer can handle.
        * t is the time set by the heuristic. 
        *
        */
        
            uint32_t alternate = k & 1;
            ComplexMatrix& new_pa = *pa[alternate];
            ComplexMatrix& old_pa = *pa[1 - alternate];


            if (k > 0)
            {
                new_pa.compress_matrix_storage();
                old_pa.compress_matrix_storage();
									
                old_pa.mul_herm_for_e_minus_i(*this, new_pa);                
						}	
						
            complex_t one_over_k_factorial_simd;
            
            /* Set symmetrical element */
            //printf("plus_minus = %u\n", plus_minus);
            if(plus_minus == true) // plus_minus = true -> (+i) 
            {
						  if((k)%4 == 0 ) // k=0,4,8...
						  {
							  one_over_k_factorial_simd = to_complex(scale_time_over_k_factorial, 0.0); 
						  }
						  else if ( (k+3)%4 ==0) // k = 1,5,9
						  {
							  one_over_k_factorial_simd = to_complex(0.0, 1.0*scale_time_over_k_factorial); 
						  }
						  else if ((k+2)%4 == 0) // k = 2,6,10
						  {
							  one_over_k_factorial_simd = to_complex(-1.0*scale_time_over_k_factorial, 0.0); 
						  }
						  else if ((k+1)%4 == 0 ) // k =3, 7, 11
						  {
							  one_over_k_factorial_simd = to_complex(0.0, -1.0*scale_time_over_k_factorial); 
						  }					
            }
            else
            { // plus_minus = false -> (-i)
						  if((k)%4 == 0 ) // k=0,4,8...
						  {
							  one_over_k_factorial_simd = to_complex(scale_time_over_k_factorial, 0.0); 
						  }
						  else if ( (k+3)%4 ==0) // k = 1,5,9
						  {
							  one_over_k_factorial_simd = to_complex(0.0, -1.0*scale_time_over_k_factorial); 
						  }
						  else if ((k+2)%4 == 0) // k = 2,6,10
						  {
							  one_over_k_factorial_simd = to_complex(-1.0*scale_time_over_k_factorial, 0.0); 
						  }
						  else if ((k+1)%4 == 0 ) // k =3, 7, 11
						  {
							  one_over_k_factorial_simd = to_complex(0.0, 1.0*scale_time_over_k_factorial); 
						  }					
						  else 
						  {
							  printf("k = %u doesn't meet criteria.\n", k);
						  }
            }
    
            if(!std::isfinite(scale_time_over_k_factorial) || k_fact < 1e-300)
            {
            /* 
            * If values are intractable using double floating point precision,
            * fail the process and the function returns 1 to indicate failure.
            */
            	done = true;
            	infinite_val = true;
            }
            else if (scale_time_over_k_factorial < precision)
            {
            	done = true;
            }
            
            else
            { /* only add to destination matrix if not yet at inf */
		          dst.add_complex_scaled_hermitian(new_pa, one_over_k_factorial_simd);
            }
            
        }
        else
        {
            done = true;
        }
    }
    dst.compress_matrix_storage();
    if(PRINT_LINE_DEBUG) printf("Line %d in file %s \n", __LINE__, __FILE__);

    return infinite_val;
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
