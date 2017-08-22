# QHL_Exponentiation
Exponentiation functionality for Quantum Hamiltonian Learning. 


To install this suite, clone the repository, go to the folder. Then type:
``` 
sudo pip install -e . 
```

Within Python: 
```
import hamiltonian_exponentiation
```

## Available functions
1. ```random_hamiltonian(num_qubits)```
  * gives a Hamiltonian matrix generated by random tensor products of Paulis.
  
1. ```exp_minus_i_h_t(hamiltonian, time, precision)```
  * Computes e^{-iHt}
  * hamiltonian: Hamiltonian matrix to be exponentiated. Hamiltonian MUST be Hermitian, otherwise this method will fail.
  * time: time generated by heuristic.
  * precision: when matrix elements are changed by this amount or smaller, exponenitation is truncated.

