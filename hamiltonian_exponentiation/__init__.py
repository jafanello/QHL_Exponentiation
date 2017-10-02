""" 
Functions which are callable once hamiltonian_exponentiation has been imported.
- exp_ham: computes exp^{iHt} or exp^{-iHt}.
- random_hamiltonian: returns a Hamiltonian randomly generated by Pauli matrices. 
"""

def exp_ham(src, t, plus_or_minus = -1.0, precision=1e-18, scalar_cutoff = 25, print_method=False, trotterize_by=1.0):
    from numpy import linalg as nplg
    n = trotterize_by
    if n == 1.0: 
      if print_method: 
        print ("Not Trotterized")
      exp_iHt = exponentiate_ham(src, t, precision=precision, scalar_cutoff=scalar_cutoff, print_method=print_method)
    else: 
      if print_method: 
        print("Trotterized; trot = ", n)
      time_over_n = t/trotterize_by
      exp_iHt_over_n = exponentiate_ham(src, time_over_n, precision=precision, scalar_cutoff=scalar_cutoff, print_method=print_method)
      exp_iHt = nplg.matrix_power(exp_iHt_over_n, n)
    return exp_iHt

def exponentiate_ham(src, t, plus_or_minus = -1.0, precision=1e-18, scalar_cutoff = 25, print_method=False):
    """
    Calls on C++ function to compute e^{-iHt}.
    Provide parameters: 
    - src: Hamiltonian to exponentiate
    - t: time
    - plus_or_mins: 1.0 to compute e^{iHt}; -1.0 to compute e^{-iHt}. Default -1.
    - precision: when matrix elements are changed by this amount or smaller, exponenitation is truncated.
    """
    import libmatrix_utils as libmu
    import numpy as np
    max_element = np.max(np.abs(src))
    new_src = src/max_element
    scalar = max_element * t
    if(scalar > scalar_cutoff):
      import scipy
      from scipy import linalg
      if(print_method):
        print("Time = ", t, "\t element = ", max_element, "\t Scalar = ", scalar, " \t Linalg (scalar).")
      if(np.shape(src)[0] > 63): # Large matrices -- worth using sparse.linalg
        from scipy import sparse
        from scipy.sparse import linalg
        from scipy.sparse import csc_matrix
        expd_mtx = scipy.sparse.linalg.expm(scipy.sparse.csc_matrix(plus_or_minus*1.j*src*t))      
      else: 
        expd_mtx = scipy.linalg.expm(plus_or_minus*1.j*src*t)
      return expd_mtx
    else:
      dst = np.ndarray(shape=(np.shape(src)[0], np.shape(src)[1]), dtype=np.complex128)
      inf_reached = libmu.exp_pm_ham(new_src, dst, plus_or_minus, scalar, precision) # Call to C++ custom exponentiation function
      if(inf_reached):
        import scipy
        from scipy import linalg
        if(np.shape(src)[0] > 63): # Large matrices -- worth using sparse.linalg
          from scipy import sparse
          from scipy.sparse import linalg
          from scipy.sparse import csc_matrix
          expd_mtx = scipy.sparse.linalg.expm(scipy.sparse.csc_matrix(plus_or_minus*1.j*src*t))      
        else: 
          expd_mtx = scipy.linalg.expm(plus_or_minus*1.j*src*t)
          if(print_method):
            print("Time = ", t, "\t element = ", max_element, "\t Scalar = ", scalar, " \t Linalg (inf).")
        return expd_mtx
      else:
        if(print_method):
          print("Time = ", t, "\t element = ", max_element, "\t Scalar = ", scalar, " \t Custom.")
        return dst

def random_hamiltonian(number_qubits):
  """
  Generate a random Hamiltonian - will be square with length/width= 2**number_qubits.
  Hamiltonian will be Hermitian so diagonally symmetrical elements are complex conjugate.
  Hamiltonian also formed by Pauli matrices.
  """
  import numpy as np
  sigmax = np.array([[0+0j, 1+0j], [1+0j, 0+0j]])
  sigmay = np.array([[0+0j, 0-1j], [0+1j, 0+0j]])
  sigmaz = np.array([[1+0j, 0+0j], [0+0j, -1+0j]])
  
  oplist =  [sigmax, sigmay, sigmaz]
  # oplist = [sigmax(), sigmay(), sigmaz()]
  size = number_qubits
  select=np.round((len(oplist)-1)*np.random.rand(size))
  newoplist = [oplist[int(i)] for i in select]
  params=np.random.rand(size)
  
  if len(params)==1:
    output = params[0]*newoplist[0]
  else:      
    for i in range(len(params)-1):
        if i==0:
            output = np.kron(params[i]*newoplist[i], params[i+1]*newoplist[i+1])
        else:
            output = np.kron(output, params[i+1]*newoplist[i+1])
    output = np.reshape(output, [2**size,2**size])
  # print('Ratio of non zero to total elements:', np.count_nonzero(output), ':', ((2**size)**2))
  return output
