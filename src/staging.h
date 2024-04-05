#ifndef STAGING_H
#define STAGING_H

#include <arrayQueue.h>

void staging_init(hid_t dataset);
void staging_deinit();

static herr_t staging_read_into_cache_memory_optimized(hid_t dset_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id);
static herr_t staging_read_into_cache_disk_optimized(hid_t dset_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id);
static herr_t H5D__staging_read_into_cache_line_format(hid_t dset_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id);
static void staging_get_chunked_dimensions(hid_t dataspace, hsize_t* start, hsize_t* end, hsize_t* size);
static hsize_t staging_get_linear_address(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank, uint8_t typeSize);
static hsize_t staging_get_linear_index(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank);
static hsize_t staging_ceiled_division(hsize_t dividend, hsize_t divisor);
static void* staging_allocate_memory(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank, uint8_t typeSize);
static void* staging_get_memory(hsize_t* coordinates, hsize_t rank);
static herr_t staging_read_from_cache(void* buffer, uint8_t typeSize, hid_t file_space_id, hid_t mem_space_id);
static herr_t staging_read_from_cache_line_format(void* buffer, uint8_t typeSize, hid_t file_space_id, hid_t mem_space_id);
static hsize_t min(hsize_t a, hsize_t b);

typedef enum {LRU, FIFO} eviction_strategy_t;
typedef enum {SQUARE, LINE} cache_shape_t;

static ArrayQueue staging_chunks;
static hsize_t staging_sizes[10] = { 0 };
static hsize_t staging_rank = 0;
static int staging_chunk_size = 1024;
static hsize_t staging_current_occupation = 0;
static hsize_t staging_cache_limit = 4ULL*1024*1024*1024;
static eviction_strategy_t staging_eviction_strategy = LRU;
static cache_shape_t staging_cache_shape = SQUARE;

void staging_init(hid_t dataset)
{    
    hid_t dataspace = H5Dget_space(dataset);
    staging_rank = H5Sget_simple_extent_ndims(dataspace);

    char* chunk_size = getenv("STAGING_CHUNK_SIZE");
    if (chunk_size != NULL)
    {
        staging_chunk_size = atoll(chunk_size);
    }    
    
    char* cache_limit = getenv("STAGING_CACHE_LIMIT");
    if (cache_limit != NULL)
    {
        staging_cache_limit = atoll(cache_limit);
    }    

    char* eviction_strategy = getenv("STAGING_EVICTION_STRATEGY");
    if (eviction_strategy != NULL)
    {
        if (strncmp(eviction_strategy, "LRU", 3) == 0)
        {            
            staging_eviction_strategy = LRU;
        }
        
        if (strncmp(eviction_strategy, "FIFO", 4) == 0)
        {            
            staging_eviction_strategy = FIFO;
        }        
    }
    
    char* cache_shape = getenv("STAGING_CACHE_SHAPE");
    if (cache_shape != NULL)
    {
        if (strncmp(cache_shape, "SQUARE", 6) == 0)
        {            
            staging_cache_shape = SQUARE;
        }
        
        if (strncmp(cache_shape, "LINE", 3) == 0)
        {            
            staging_cache_shape = LINE;
        }        
    }

    if (staging_rank == 2)
    {
        hsize_t sizes[2];
        H5Sget_simple_extent_dims(dataspace, sizes, NULL);  
        if (staging_cache_shape == SQUARE)
        {
            staging_sizes[0] = staging_ceiled_division(sizes[0], staging_chunk_size);
            staging_sizes[1] = staging_ceiled_division(sizes[1], staging_chunk_size);
            arrayQueue_init(&staging_chunks, staging_sizes[0] * staging_sizes[1]);
        }
        else
        {
            staging_sizes[0] = sizes[1];
            arrayQueue_init(&staging_chunks, staging_sizes[0]);
        }        
        staging_current_occupation = 0;
    }
}

void staging_deinit()
{
    arrayQueue_deinit(&staging_chunks);
}

herr_t staging_read_into_cache_memory_optimized(hid_t dset_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id)
{                    
    hid_t datatype = H5Dget_type(dset_id);
    size_t typeSize = H5Tget_size(datatype);
    
    hsize_t file_space_size[2];
    H5Sget_simple_extent_dims(file_space_id, file_space_size, NULL);

    hsize_t mem_space_offset[] = { 0, 0 };
    hsize_t mem_space_dimensions[] = { staging_chunk_size, staging_chunk_size };
    hsize_t mem_space_size[] = { min(mem_space_dimensions[0], file_space_size[0]), min(mem_space_dimensions[1], file_space_size[1]) };
    hid_t mem_space = H5Screate_simple(2, mem_space_dimensions, NULL);
    H5Sselect_hyperslab(mem_space, H5S_SELECT_SET, mem_space_offset, NULL, mem_space_size, NULL);

    hsize_t chunked_start[2];
    hsize_t chunked_end[2];
    hsize_t chunked_size[2];
    staging_get_chunked_dimensions(file_space_id, chunked_start, chunked_end, chunked_size);

    for (size_t j = chunked_start[0]; j < chunked_end[0]; ++j)
    {
        for (size_t i = chunked_start[1]; i < chunked_end[1]; ++i)
        {
            hsize_t coordinates[] = {j, i};
            void* staged_data = staging_get_memory(coordinates, staging_rank);
            if (staged_data == NULL)
            {
                staged_data = staging_allocate_memory(coordinates, staging_sizes, staging_rank, typeSize);
                hsize_t offset[] = { j*staging_chunk_size, i*staging_chunk_size };
                hid_t file_space = H5Scopy(file_space_id);
                H5Sselect_hyperslab(file_space, H5S_SELECT_SET, offset, NULL, mem_space_size, NULL);

                herr_t error = H5D__read_api_common(1, &dset_id, &mem_type_id, &mem_space, &file_space, dxpl_id, &staged_data, NULL, NULL);
                if (error < 0) return error;                
            }
        }
    }    

    return SUCCEED;
}

herr_t staging_read_into_cache_disk_optimized(hid_t dset_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id)
{   
    hid_t datatype = H5Dget_type(dset_id);
    size_t typeSize = H5Tget_size(datatype);

    hid_t file_space = H5Scopy(file_space_id);

    hsize_t chunked_start[2];
    hsize_t chunked_end[2];
    hsize_t chunked_size[2];
    staging_get_chunked_dimensions(file_space, chunked_start, chunked_end, chunked_size);
    
    hsize_t intermediate_space_offset[] = { 0, 0 };
    hsize_t intermediate_space_size[] = { chunked_size[0] * staging_chunk_size, chunked_size[1] * staging_chunk_size };
    hid_t intermediate_space = H5Screate_simple(2, intermediate_space_size, NULL);

    void* intermediate_buffer = malloc(intermediate_space_size[0] * intermediate_space_size[1] * typeSize);

    for (size_t j = 0; j < chunked_size[0]; ++j)
    {
        for (size_t i = 0; i < chunked_size[1]; ++i)
        {
            hsize_t chunked_index[] = {chunked_start[0] + j, chunked_start[1] + i};
            void* staged_data = staging_get_memory(chunked_index, staging_rank);
            
            hsize_t source_offset[] = {chunked_index[0] * staging_chunk_size, chunked_index[1] * staging_chunk_size};
            hsize_t intermediate_offset[] = {j * staging_chunk_size, i * staging_chunk_size};
            hsize_t size[] = {staging_chunk_size, staging_chunk_size};
            if (staged_data == NULL)
            {
                H5Sselect_hyperslab(file_space, H5S_SELECT_OR, source_offset, NULL, size, NULL);
                H5Sselect_hyperslab(intermediate_space, H5S_SELECT_OR, intermediate_offset, NULL, size, NULL);
            }
            else
            {                
                H5Sselect_hyperslab(file_space, H5S_SELECT_NOTB, source_offset, NULL, size, NULL);
                H5Sselect_hyperslab(intermediate_space, H5S_SELECT_NOTB, intermediate_offset, NULL, size, NULL);
            }            
        }
    }

    herr_t error = H5D__read_api_common(1, &dset_id, &mem_type_id, &intermediate_space, &file_space, dxpl_id, &intermediate_buffer, NULL, NULL);       
    if (error < 0) return error;
    
    hsize_t source_array_size[] = {chunked_size[0] * staging_chunk_size, chunked_size[1] * staging_chunk_size};
    hsize_t target_array_size[] = {staging_chunk_size, staging_chunk_size};

    for (size_t j = 0; j < chunked_size[0]; ++j)
    {
        for (size_t i = 0; i < chunked_size[1]; ++i)
        {
            hsize_t chunked_index[] = {chunked_start[0] + j, chunked_start[1] + i};
            void* staged_data = staging_get_memory(chunked_index, staging_rank);
            if (staged_data == NULL)
            {
                staged_data = staging_allocate_memory(chunked_index, staging_sizes, staging_rank, typeSize);
                for (size_t k = 0; k < staging_chunk_size; k++)
                {
                    hsize_t source_coordinates[] =  {j*staging_chunk_size + k, i*staging_chunk_size};
                    void* source = intermediate_buffer + staging_get_linear_address(source_coordinates, source_array_size, 2, typeSize);
                    hsize_t target_coordinates[] = {k, 0};
                    void* target = staged_data + staging_get_linear_address(target_coordinates, target_array_size, 2, typeSize);
                    memcpy(target, source, staging_chunk_size * typeSize);
                }                
            }
        }
    }

    free(intermediate_buffer);
    return SUCCEED;
}

herr_t H5D__staging_read_into_cache_line_format(hid_t dset_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, hid_t mem_type_id)
{
    herr_t ret_value = SUCCEED; /* required for macro instrumentation */
    
    FUNC_ENTER_PACKAGE


    hid_t datatype = H5Dget_type(dset_id);
    size_t type_size = H5Tget_size(datatype);

    hsize_t file_space_start[2];
    hsize_t file_space_count[2];
    H5Sget_regular_hyperslab(file_space_id, file_space_start, NULL, file_space_count, NULL);
    
    hsize_t file_space_size[2];
    H5Sget_simple_extent_dims(file_space_id, file_space_size, NULL);

    hsize_t start_line = file_space_start[0];
    hsize_t end_line = file_space_start[0] + file_space_count[0];

    
    hsize_t cache_space_offset[] = { 0, 0 };
    hsize_t cache_space_size[] = { 1, file_space_size[1] };
    hid_t cache_space = H5Screate_simple(2, cache_space_size, NULL);
    H5Sselect_hyperslab(cache_space, H5S_SELECT_SET, cache_space_offset, NULL, cache_space_size, NULL);

    for (size_t i = start_line; i < end_line; ++i)
    {
        hsize_t coordinates[] = {i};
        void* staged_data = staging_get_memory(coordinates, 1);
        if (staged_data == NULL)
        {
            staged_data = staging_allocate_memory(coordinates, staging_sizes, 1, type_size);
            hsize_t offset[] = { i, 0 };
            hid_t file_space = H5Scopy(file_space_id);
            hsize_t size[] = {1, file_space_size[1]};
            H5Sselect_hyperslab(file_space, H5S_SELECT_SET, offset, NULL, size, NULL);

            if (H5D__read_api_common(1, &dset_id, &mem_type_id, &cache_space, &file_space, dxpl_id, &staged_data, NULL, NULL) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "read failed for staging")
        }
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
}

void staging_get_chunked_dimensions(hid_t dataspace, hsize_t* start, hsize_t* end, hsize_t* size)
{                
    hsize_t dataspace_start[2];
    hsize_t dataspace_stride[2];
    hsize_t dataspace_count[2];
    hsize_t dataspace_block[2];
    H5Sget_regular_hyperslab(dataspace, dataspace_start, dataspace_stride, dataspace_count, dataspace_block);

    start[0] = (dataspace_start[0] / staging_chunk_size);
    start[1] = (dataspace_start[1] / staging_chunk_size);
    
    end[0] = staging_ceiled_division(dataspace_start[0] + dataspace_count[0], staging_chunk_size);
    end[1] = staging_ceiled_division(dataspace_start[1] + dataspace_count[1], staging_chunk_size);
    
    size[0] = end[0] - start[0];
    size[1] = end[1] - start[1];
}

hsize_t staging_ceiled_division(hsize_t dividend, hsize_t divisor)
{
    hsize_t quotient = dividend / divisor;
    hsize_t remainder = dividend % divisor;

    if (remainder > 0)
    {
        return quotient + 1;
    }
    else
    {
        return quotient;
    }
}

void* staging_allocate_memory(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank, uint8_t typeSize)
{
    hsize_t index = staging_get_linear_index(coordinates, array_dimensions, rank);
    Node* node = arrayQueue_get_by_index(&staging_chunks, index);
    arrayQueue_move_to_front(&staging_chunks, node);
    void* chunk;
    
    if (staging_current_occupation < staging_cache_limit)
    {
        hsize_t size;
        if (staging_cache_shape == SQUARE)
        {
            size = staging_chunk_size * staging_chunk_size * typeSize;
        }
        else if (staging_cache_shape == LINE)
        {
            size = array_dimensions[0] * typeSize;
        }
        
        chunk = malloc(size);    
        staging_current_occupation += size;
    }
    else
    {
        Node* tail = arrayQueue_get_tail(&staging_chunks);
        chunk = tail->memory;
        tail->memory = NULL;
        if (tail != node)
        {            
            arrayQueue_pop_tail(&staging_chunks);
        }        
    }
    node->memory = chunk;

    return chunk;
}

void* staging_get_memory(hsize_t coordinates[], hsize_t rank)
{
    hsize_t index = staging_get_linear_index(coordinates, staging_sizes, rank);
    Node* node = arrayQueue_get_by_index(&staging_chunks, index);  

    if (staging_eviction_strategy == LRU)
    {        
        arrayQueue_move_to_front(&staging_chunks, node);
    }
      
    return node->memory;
}

herr_t staging_read_from_cache(void* buffer, uint8_t typeSize, hid_t file_space_id, hid_t mem_space_id)
{
    volatile hsize_t source_start[2];
    volatile hsize_t source_stride[2];
    volatile hsize_t source_count[2];
    volatile hsize_t source_block[2];
    H5Sget_regular_hyperslab(file_space_id, source_start, source_stride, source_count, source_block);
    volatile hsize_t source_end[2];
    source_end[0] = source_start[0] + source_count[0];
    source_end[1] = source_start[1] + source_count[1];

    hsize_t source_chunked_start[2];    
    hsize_t source_chunked_end[2];
    hsize_t source_chunked_size[2];
    staging_get_chunked_dimensions(file_space_id, source_chunked_start, source_chunked_end, source_chunked_size);

    hsize_t source_array_size[2];
    source_array_size[0] = staging_chunk_size;
    source_array_size[1] = staging_chunk_size;
    
    hsize_t target_start[2];
    hsize_t target_stride[2];
    hsize_t target_count[2];
    hsize_t target_block[2];
    H5Sget_regular_hyperslab(mem_space_id, target_start, target_stride, target_count, target_block);
    hsize_t target_end[2];
    target_end[0] = target_start[0] + target_count[0];
    target_end[1] = target_start[1] + target_count[1];

    hsize_t target_chunked_start[2];
    hsize_t target_chunked_end[2];
    hsize_t target_chunked_size[2];
    staging_get_chunked_dimensions(file_space_id, target_chunked_start, target_chunked_end, target_chunked_size);

    hsize_t target_array_size[2];
    hsize_t target_array_maxsize[2];
    H5Sget_simple_extent_dims(mem_space_id, target_array_size, target_array_maxsize);

    hsize_t target_coordinates[] = {target_start[0], target_start[1]};;

    for (size_t j = source_chunked_start[0]; j < source_chunked_end[0]; ++j)
    {
        target_coordinates[1] = target_start[1]; //?
        
        hsize_t start_row = 0;
        hsize_t end_row = staging_chunk_size;
        hsize_t row_count = staging_chunk_size;
        if (j == source_chunked_start[0])
        {
            start_row = source_start[0] % staging_chunk_size;
            row_count = staging_chunk_size - start_row;
        }

        if (j == source_chunked_end[0] - 1)
        {
            end_row = source_end[0] % staging_chunk_size;
            end_row = (end_row != 0) ? end_row : staging_chunk_size;
            row_count = end_row - start_row;
        }    

        for (size_t i = source_chunked_start[1]; i < source_chunked_end[1]; ++i)
        {                    
            hsize_t position_in_row = 0;
            hsize_t row_size = staging_chunk_size;
            if (i == source_chunked_start[1])
            {
                position_in_row = source_start[1] % staging_chunk_size;
                row_size = staging_chunk_size - position_in_row;
            }

            if (i == source_chunked_end[1] - 1)
            {
                row_size = (source_end[1] - position_in_row) % staging_chunk_size;
                row_size = (row_size != 0) ? row_size : staging_chunk_size;
            }            

            for (size_t k = start_row; k < end_row; ++k)
            {
                hsize_t chunk_index[] = {j, i};
                hsize_t source_coordinates[] = {k, position_in_row};
                void* base = staging_get_memory(chunk_index, staging_rank);
                if (base == NULL) continue; // null if cache eviction occurred
                void* source = base + staging_get_linear_address(source_coordinates, source_array_size, staging_rank, typeSize);
                void* target = buffer + staging_get_linear_address(target_coordinates, target_array_size, staging_rank, typeSize);

                memcpy(target, source, row_size * typeSize);
                target_coordinates[0] += 1;
                //printf("-");
            }

            target_coordinates[0] -= row_count;
            target_coordinates[1] += row_size;
        }

        target_coordinates[0] += row_count;
    }    

    return SUCCEED;
}

herr_t staging_read_from_cache_line_format(void* buffer, uint8_t typeSize, hid_t file_space_id, hid_t mem_space_id)
{        
    //hid_t dataset_space = H5Dget_space(dset_id);
    hsize_t file_space_size[2];
    H5Sget_simple_extent_dims(file_space_id, file_space_size, NULL);
    
    hsize_t file_space_start[2];
    hsize_t file_space_count[2];
    H5Sget_regular_hyperslab(file_space_id, file_space_start, NULL, file_space_count, NULL);

    hsize_t start_line = file_space_start[0];
    hsize_t line_count = file_space_count[0];
    hsize_t start_column = file_space_start[1];

    hsize_t target_array_start[2];
    hsize_t target_array_count[2];
    H5Sget_regular_hyperslab(mem_space_id, target_array_start, NULL, target_array_count, NULL);

    hsize_t target_array_size[2];
    H5Sget_simple_extent_dims(mem_space_id, target_array_size, NULL);

    for (size_t i = 0; i < line_count; ++i )
    {
        hsize_t source_array_size[] = { file_space_size[1] };
        hsize_t source_index[] = { start_line + i };
        hsize_t source_coordinates[] = { start_column };
        void* base = staging_get_memory(source_index, 1);
        if (base == NULL) continue; // null if cache eviction occurred
        void* source = base + staging_get_linear_address(source_coordinates, source_array_size, 1, typeSize);
        
        hsize_t target_coordinates[] = {target_array_start[0] + i, target_array_start[1]};
        void* target = buffer + staging_get_linear_address(target_coordinates, target_array_size, 2, typeSize);

        memcpy(target, source, target_array_count[1] * typeSize);
    }

    return SUCCEED;
}

hsize_t staging_get_linear_address(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank, uint8_t typeSize)
{
    return staging_get_linear_index(coordinates, array_dimensions, rank) * typeSize;
}

hsize_t staging_get_linear_index(hsize_t* coordinates, hsize_t* array_dimensions, hsize_t rank)
{
    hsize_t address = 0;
    hsize_t multiplier = 1;
    for (int i = rank - 1; i >= 0; --i)
    {
        address += coordinates[i] * multiplier;
        multiplier *= array_dimensions[i];
    }

    return address;
}

hsize_t min(hsize_t a, hsize_t b)
{
    return (a < b) ? a : b;
}
#endif