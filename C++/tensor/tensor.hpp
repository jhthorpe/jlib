/*----------------------------------------------------------------------------
  tensor.hpp
	JHT, April 10, 2022 : created

  .hpp file for the general tensor class. This behaves similarly to 
  std::array in that it cannot be grown dynamically, though it can be 
  allocated (similar to a fortran dimension). 

  The storage is column major

  Note that the index access via () is implemented via a varadic template, 
  and thus should be rather efficient, though [], which accesses based on the
  stride should still be prefered

  INITIALIZATION
  -------------------
  Create, but do not assign or allocate
    libj::tensor<double> T; 

  Create and allocate with set legnths via malloc
    libj::tensor<double> A(1,4,3);

  Create and assign to memory with set lengths
    libj::tensor<double> A(pointer,1,4,3);

  Allocate or assign an existing tensor:
    T.allocate(1,4,3);
    T.aligned_allocate(64,1,4,3); //where 64 is the byte alignment 
    T.assign(1,4,3,pointer);

  Deallocate
    T.deallocate();
    T.unassign();

  Reassignment (including reshaping)
    T.assign(pointer, 2,5,1);
  

  ELEMENT ACCESS
  ------------------
  To access the n'th element
    T[N];

  To use the index access
    T(1,2,3);


  USEFUL FUNCTIONS
  --------------------
    same_shape<double>(T1,T2);	//returns true if T1 and T2 (double tensors) 
            			//  are the same shape, false otherwise
    dim();			//returns number of dimensions
    size();			//returns total number of elements
    size(2);			//returns size of dimension 2 (3rd dimension)
    stride(1);			//returns stride of dimension 1 (2nd dimension)
    is_allocated();		//returns true if allocated
    is_assigned();		//returns true if assigned
    is_set();			//returns true if allocated or assigned
----------------------------------------------------------------------------*/
#ifndef TENSOR_HPP
#define TENSOR_HPP

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>
//#include "alignment.hpp"

//Standard alignment
#define DEFAULT_ALIGN 16

//Macro for making index array from varadic list of ints
#define make_index(dims,i0)\
  dims[0] = i0; \
  va_list args; va_start(args,i0); \
  for (size_t i=1;i<N;i++) {dims[i] = va_arg(args,size_t);} \
  va_end(args);

namespace libj 
{

static size_t calc_alignment(const void* pointer)
{
  size_t alignment = 1;

  //2,4,8,16,32,64,128,256,512
  for (long m=2;m<=512;m*=2)
  {
    if ((long)pointer%m != 0) {return alignment;}
    alignment *= 2;
  }
  return alignment;
}

template<typename T>
class tensor
{
  private:
  T*                  M_BUFFER;       //start of data
  T*                  M_POINTER;      //pointer to malloc	
  std::vector<size_t> M_LENGTHS;      //vector of lengths 
  std::vector<size_t> M_STRIDE;      //pointer to strides
  size_t	      M_NDIM;         //number of dimensions
  size_t              M_NELM;      //total number of elements
  size_t              M_ALIGNMENT;    //alignment in bytes
  bool                M_IS_ALLOCATED; //tensor is allocated with malloc 
  bool                M_IS_ASSIGNED;  //tensor is assigned with malloc 

  //internal functions
  void m_set_default();
  void m_allocate();
  void m_aligned_allocate(const size_t BYTES);
  void m_assign(T* pointer);

  //internal varadic templates for data access
  template<class...Rest>
  size_t m_index(const size_t level, const size_t first, const Rest...rest) const
  {
    return first*M_STRIDE[level] + m_index(level+1,rest...); 
  }
  size_t m_index(const size_t level, const size_t first) const
  {
    return first*M_STRIDE[level];
  }

  //internal varadic templates for initialization
  void m_init()
  {
    M_NDIM = M_LENGTHS.size();
    for (size_t i=0;i<M_NDIM;i++)
    {
      if (M_LENGTHS[i] <= 0)
      {
        printf("ERROR libj::tensor::m_init\n");
        printf("Dimension %zu has length <= 0 \n",i);
        exit(1);
      }
    }
  }
  void m_init(const size_t first)
  {
    M_STRIDE.push_back(M_NELM);
    M_LENGTHS.push_back(first);
    M_NELM *= first;
    m_init();
  }
  template <class...Rest> void m_init(const size_t first, const Rest...rest)
  {
    M_STRIDE.push_back(M_NELM);
    M_LENGTHS.push_back(first);
    M_NELM *= first;
    m_init(rest...);
  }
  
  public:

  //Constructors and destructors
  tensor();			
  ~tensor();			

  //Allocate constructor
  template<class...Rest> tensor(const size_t first,const Rest...rest);	

  //Assign constructor
  template<class...Rest> tensor(T* pointer, const size_t first,const Rest...rest);

  //allocate,assign
  template<class...Rest> void allocate(const size_t first,const Rest...rest);
  template<class...Rest> void aligned_allocate(const size_t BYTES, const size_t first,
                                               const Rest...rest);
  template<class...Rest> void assign(T* pointer, const size_t first,const Rest...rest);
  void deallocate();
  void unassign();

  //Getters
  const size_t  size() const {return M_NELM;}
  const size_t  size(const size_t dim) const {return M_LENGTHS[dim];}
  const size_t  dim() const {return M_NDIM;}
  const size_t  alignment() const {return M_ALIGNMENT;}
  const size_t  stride(const size_t dim) const {return M_STRIDE[dim];}
  const bool    is_allocated() const {return M_IS_ALLOCATED;}
  const bool    is_assigned() const {return M_IS_ASSIGNED;}
  const bool    is_set() const {return M_IS_ALLOCATED || M_IS_ASSIGNED;}

  //Access functions
  T& operator[] (const size_t stride) {return *(M_BUFFER+stride);}
  const T& operator[] (const size_t stride) const {return *(M_BUFFER+stride);}

  template<class...Rest> T& operator() (const size_t i0,const Rest...rest)
  {
    return *(M_BUFFER + i0*M_STRIDE[0] + m_index(1,rest...));
  }

  template<class...Rest> const T& operator() (const size_t i0,const Rest...rest) const
  {
    return *(M_BUFFER + i0*M_STRIDE[0] + m_index(1,rest...));
  }

}; //end of normal tensor

//-----------------------------------------------------------------------
// returns true if the tensors are the same shape
//-----------------------------------------------------------------------
template <typename T>
bool same_shape(const tensor<T>& A, const tensor<T>& C)
{
  if (A.dim() != C.dim()) {return false;}
  for (int dim=0;dim<A.dim();dim++)
  {
    if (A.size(dim) != C.size(dim)) {return false;}
  }
  return true;
}

//-----------------------------------------------------------------------
// Initialization functions
//-----------------------------------------------------------------------
template<typename T>
void tensor<T>::m_set_default()
{ 
  M_BUFFER = NULL;
  M_POINTER = NULL;
  M_NELM = 0; 
  M_ALIGNMENT = 0; 
  M_IS_ALLOCATED = false;
  M_IS_ASSIGNED = false;
}

//-----------------------------------------------------------------------
//Blank contructor
//-----------------------------------------------------------------------
template<typename T>
tensor<T>::tensor()
{
  m_set_default();
}

//-----------------------------------------------------------------------
// blank destructor
//-----------------------------------------------------------------------
template<typename T>
tensor<T>::~tensor()
{
  if (M_IS_ALLOCATED) deallocate();
}

//-----------------------------------------------------------------------
//Allocator constructor
//-----------------------------------------------------------------------
template<typename T> template<class...Rest>
tensor<T>::tensor(const size_t first,const Rest...rest)
{
  //set the default
  m_set_default();

  //initialize
  M_LENGTHS.push_back(first);
  M_STRIDE.push_back(1);
  M_NELM = first; 
  m_init(rest...);//performs rest of varadic initialization

  //call the internal allocate function
  m_allocate();
}

//-----------------------------------------------------------------------
//Assignment constructor
//-----------------------------------------------------------------------
template<typename T> template<class...Rest>
tensor<T>::tensor(T* pointer, const size_t first,const Rest...rest)
{
  //set the default
  m_set_default();

  //Initialize
  M_LENGTHS.push_back(first);
  M_STRIDE.push_back(1);
  M_NELM = first; 
  m_init(rest...);//performs rest of varadic initialization

  //call the internal allocate function
  m_assign(pointer);
}

//-----------------------------------------------------------------------
// internal malloc
//-----------------------------------------------------------------------
template<typename T>
void tensor<T>::m_allocate()
{
  if (!M_IS_ALLOCATED && !M_IS_ASSIGNED)
  {
    M_POINTER = (T*) malloc(sizeof(T)*M_NELM);
    M_BUFFER = M_POINTER;
    if (M_BUFFER == NULL || M_POINTER == NULL)
    {
      printf("ERROR libj::tensor::m_allocate could not allocate M_BUFFER \n");
      exit(1);
     }
     M_IS_ALLOCATED = true;
     M_ALIGNMENT = libj::calc_alignment((void*) M_BUFFER);

   } else {
     printf("ERROR libj::tensor::m_allocate\n");
     printf("attempted to allocate an already set vector\n");
     exit(1);
   }
}

//-----------------------------------------------------------------------
// internal assign
//-----------------------------------------------------------------------
template<typename T>
void tensor<T>::m_assign(T* pointer)
{
  if (!M_IS_ALLOCATED)
  {
    M_BUFFER = pointer;
    M_POINTER = pointer;
    M_IS_ASSIGNED = true;
    M_ALIGNMENT = libj::calc_alignment((void*) M_BUFFER);
  } else {
    printf("ERROR libj::tensor::m_assign\n");
    printf("attempted to assign an already allocated vector\n");
    exit(1);
   }
}

//----------------------------------------------------------------------------
// internal aligned_allocate 
//----------------------------------------------------------------------------
template<typename T>
void libj::tensor<T>::m_aligned_allocate(const size_t ALIGN)
{
  //check we are not allocated or assigned
  if (!M_IS_ALLOCATED && !M_IS_ASSIGNED)
  {
    //check alignment is divisible by 2
    if (ALIGN%2 != 0)
    {
      printf("ERROR libj::tensor::m_aligned_allocate\n");
      printf("input BYTE ALIGN was not divisible by 2 \n");
      exit(1);
    }

    //align the buffer pointer  
    M_POINTER = (T*) malloc(ALIGN+M_NELM*sizeof(T));
    if (M_POINTER != NULL)
    {
      long M = (long)M_POINTER%(long)ALIGN; //number of bytes off
      if (M != 0)
      {
        //shift M_BUFFER start to match boundary
        char* cptr = (char*) M_POINTER;
        for (long i=0;i<((long)ALIGN-M);i++)
        {
          ++cptr;
        }
        M_BUFFER = (T*) cptr;
      } else {
        M_BUFFER = M_POINTER;
      }
    }

    //check all went well
    if (M_BUFFER == NULL || M_POINTER == NULL)
    {
      printf("ERROR libj::tensor::m_aligned_allocate\n");
      printf("could not allocate M_BUFFER \n");
      exit(1);
    }
    M_IS_ALLOCATED = true;
    M_ALIGNMENT = calc_alignment(M_BUFFER);

  } else {
    printf("ERROR libj::tensor::m_aligned_allocate\n");
    printf("attempted to allocate an already set vector\n");
    exit(1);
  }
}

//-----------------------------------------------------------------------
// allocate
//-----------------------------------------------------------------------
template <typename T> template<class...Rest>
void tensor<T>::allocate(const size_t first,const Rest...rest)
{
  if (!M_IS_ALLOCATED && !M_IS_ASSIGNED)
  {
    M_LENGTHS.push_back(first);
    M_STRIDE.push_back(1);
    M_NELM = first;
    m_init(rest...);
    m_allocate();
  } else {
    printf("ERROR libj::tensor::allocate\n");
    printf("attempted to allocated an already allocated tensor\n");
    exit(1);
  }
}

//-----------------------------------------------------------------------
// aligned allocate
//-----------------------------------------------------------------------
template <typename T> template<class...Rest>
void tensor<T>::aligned_allocate(const size_t BYTES, const size_t first,
                                 const Rest...rest)
{
  if (!M_IS_ALLOCATED && !M_IS_ASSIGNED)
  {
    M_LENGTHS.push_back(first);
    M_STRIDE.push_back(1);
    M_NELM = first;
    m_init(rest...);
    m_aligned_allocate(BYTES);
  } else {
    printf("ERROR libj::tensor::aligned_allocate\n");
    printf("attempted to allocated an already allocated tensor\n");
    exit(1);
  }
}

//-----------------------------------------------------------------------
// assign 
//-----------------------------------------------------------------------
template <typename T> template<class...Rest>
void tensor<T>::assign(T* pointer, const size_t first,const Rest...rest)
{
  if (!M_IS_ALLOCATED)
  {
    M_LENGTHS.push_back(first);
    M_STRIDE.push_back(1);
    M_NELM = first;
    m_init(rest...);
    m_assign(pointer);
  } else {
    printf("ERROR libj::tensor::assign\n");
    printf("attempted to assign an already allocated tensor\n");
    exit(1);
  }
}

//-----------------------------------------------------------------------
// deallocate via free 
//-----------------------------------------------------------------------
template<typename T>
void tensor<T>::deallocate()
{
  if (M_IS_ALLOCATED)
  {
    if (M_POINTER != NULL) {free(M_POINTER);}
    M_BUFFER = NULL;
  } else {
    printf("ERROR libj::tensor::deallocate \n");
    printf("attempted to deallocate an unallocated tensor \n");
    exit(1);
  }
} 

//-----------------------------------------------------------------------
// unassign
//-----------------------------------------------------------------------
template<typename T>
void tensor<T>::unassign()
{
  if (M_IS_ASSIGNED)
  {
    M_POINTER = NULL; 
    M_BUFFER = NULL;
    m_set_default(); 
  } else {
    printf("ERROR libj::tensor::unassign \n");
    printf("attempted to unassign an unassigned tensor \n");
    exit(1);
  }
} 

}//end of namespace

#endif
