# `soa::vector<T>`

The `vector<T>` container internally uses a `split_buffer` to lay out fields in a SoA(Structure-Of-Arrays) fashion.

## `split_buffer`

A `split_buffer` is a contiguous array data structure. It may hold spare capacity at both ends of the buffer. This allows a `split_buffer` to grow from both the front and the back without relocating its contents until it runs out of room.
    
The contents of the `split_buffer` are stored in a contiguous buffer allocated on the heap. For an aggregate type `T{m_0, m_1, m_2}`, each member is stored in its own sub-array, delimited by the pointers shown below.