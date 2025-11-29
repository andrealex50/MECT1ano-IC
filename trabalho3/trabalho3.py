#!/usr/bin/env python3
"""
Strategy: Lossless Bitshuffle + Zstandard
Target: Qwen2-0.5B (BFloat16 weights)
"""

import struct
import json
import zstandard as zstd
import numpy as np
from pathlib import Path
import time
import argparse
from typing import Dict, Tuple

class SafetensorsCompressor:
    """
    Uses Byte-Transposition (Bitshuffle) to optimize floating point compression.
    """
    
    MAGIC = b'STBS'  # SafeTensors BitShuffle
    VERSION = 2      # version 2 (Shuffle Strategy)
    
    def __init__(self, compression_level=3):
        """
        Args:
            compression_level: Default 3. 
                               Higher levels (19+) are too slow 
        """
        self.compression_level = compression_level
        
        # Zstandard compressor
        self.compressor = zstd.ZstdCompressor(
            level=compression_level,
            threads=-1,  # Use all CPU cores
            write_content_size=True,
            write_checksum=True
        )
        
        self.decompressor = zstd.ZstdDecompressor()
    
    def read_safetensors_structure(self, file_path: str) -> Tuple[dict, int, int, bytes]:
        """
        Parse safetensors file structure without loading all data.
        """
        with open(file_path, 'rb') as f:
            # Read header size (first 8 bytes, little-endian uint64)
            header_size_bytes = f.read(8)
            header_size = struct.unpack('<Q', header_size_bytes)[0]

            # Read header JSON bytes
            header_json_bytes = f.read(header_size)
            header_json = header_json_bytes.decode('utf-8')
            header = json.loads(header_json)

            data_offset = 8 + header_size

            return header, header_size, data_offset, header_json_bytes


    def _shuffle_bytes(self, data: bytes, dtype: str) -> bytes:
        """
        CORE ALGORITHM: Bitshuffle
        Rearranges bytes to group significant bits together.
        Example for 2-byte numbers (BF16):
        Input:  [Hi1, Lo1, Hi2, Lo2, Hi3, Lo3]
        Output: [Hi1, Hi2, Hi3, Lo1, Lo2, Lo3]
        """

        if dtype in ['BF16', 'F16', 'I16', 'U16']:
            width = 2
        elif dtype in ['F32', 'I32', 'U32']:
            width = 4
        elif dtype in ['F64', 'I64', 'U64']:
            width = 8
        else:
            return data  # do not shuffle 1-byte types (I8, U8, BOOL)


        arr = np.frombuffer(data, dtype=np.uint8)
        
        # sanity check
        if len(arr) % width != 0:
            return data
            
        arr_shuffled = arr.reshape(-1, width).T.copy()
        
        return arr_shuffled.tobytes()

    def _unshuffle_bytes(self, data: bytes, dtype: str) -> bytes:
        """
        Reverse the Bitshuffle operation to restore original byte order.
        """
        if dtype in ['BF16', 'F16', 'I16', 'U16']:
            width = 2
        elif dtype in ['F32', 'I32', 'U32']:
            width = 4
        elif dtype in ['F64', 'I64', 'U64']:
            width = 8
        else:
            return data

        arr = np.frombuffer(data, dtype=np.uint8)

        # The shape must be (width, N) because that's how we saved it
        # Reshape back to (width, N) and Transpose to (N, width)
        try:
            arr_restored = arr.reshape(width, -1).T.copy()
            return arr_restored.tobytes()
        except ValueError:
            return data

    def compress_file(self, input_path: str, output_path: str) -> Dict:
        """
        Compress a safetensors file using Bitshuffle + Zstd.
        """
        start_time = time.time()
        
        print(f"Reading structure from {input_path}...")
        header, header_size, data_offset, header_json_bytes = self.read_safetensors_structure(input_path)

        input_size = Path(input_path).stat().st_size
        
        compressed_tensors = {}
        modified_header = {}
        

        tensor_names = [k for k in header.keys() if k != '__metadata__']
        total_tensors = len(tensor_names)
        
        print(f"Processing {total_tensors} tensors with Bitshuffle + Zstd (Level {self.compression_level})...")
        
        with open(input_path, 'rb') as f:

            if '__metadata__' in header:
                modified_header['__metadata__'] = header['__metadata__']
                modified_header['__original_header__'] = header_json_bytes.decode('utf-8')
            
            for i, tensor_name in enumerate(tensor_names):
                tensor_info = header[tensor_name]
                dtype = tensor_info['dtype']
                shape = tensor_info['shape']
                data_offsets = tensor_info['data_offsets']
                
                # 1. Read tensor data
                start_offset, end_offset = data_offsets
                tensor_size = end_offset - start_offset
                
                f.seek(data_offset + start_offset)
                tensor_data = f.read(tensor_size)

                
                # 2. Apply Bitshuffle (Optimize structure for compression)
                shuffled_data = self._shuffle_bytes(tensor_data, dtype)
                
                # 3. Compress
                compressed = self.compressor.compress(shuffled_data)
                compressed_tensors[tensor_name] = compressed

                
                # 4. Update Header
                modified_header[tensor_name] = {
                    'dtype': dtype,
                    'shape': shape,
                    'compressed_size': len(compressed),
                    'original_size': len(tensor_data)
                }
                

                if (i + 1) % 10 == 0 or (i + 1) == total_tensors:
                    print(f"  Progress: {i+1}/{total_tensors} ({100*(i+1)/total_tensors:.1f}%)", end='\r')
        
        print(f"\nWriting compressed file to {output_path}...")
        
        with open(output_path, 'wb') as f:

            f.write(self.MAGIC)
            f.write(struct.pack('<I', self.VERSION))
            
            header_json = json.dumps(modified_header, separators=(',', ':')).encode('utf-8')
            compressed_header = self.compressor.compress(header_json)
            f.write(struct.pack('<I', len(compressed_header)))
            f.write(compressed_header)
            
            for tensor_name in tensor_names:
                f.write(compressed_tensors[tensor_name])
        
        output_size = Path(output_path).stat().st_size
        compression_time = time.time() - start_time
        
        stats = {
            'input_size_mb': input_size / (1024**2),
            'output_size_mb': output_size / (1024**2),
            'ratio': input_size / output_size,
            'time': compression_time
        }
        
        print(f"\n{'='*60}")
        print(f"COMPRESSION SUCCESS")
        print(f"  Original Size:   {stats['input_size_mb']:.2f} MB")
        print(f"  Compressed Size: {stats['output_size_mb']:.2f} MB")
        print(f"  Ratio:           {stats['ratio']:.2f}x")
        print(f"  Time Taken:      {stats['time']:.2f} s")
        print(f"{'='*60}")
        
        return stats
    
    def decompress_file(self, input_path: str, output_path: str) -> Dict:
        """
        Decompress a file created by this tool.
        """
        start_time = time.time()
        print(f"Decompressing {input_path}...")
        
        with open(input_path, 'rb') as f:
            # Verify Magic
            magic = f.read(4)
            if magic != self.MAGIC:
                raise ValueError(f"Invalid file format. Expected {self.MAGIC}, got {magic}")
            
            # Verify Version
            version = struct.unpack('<I', f.read(4))[0]
            if version != self.VERSION:
                raise ValueError(f"Unsupported version {version}")
            
            # Read Header
            header_size = struct.unpack('<I', f.read(4))[0]
            compressed_header = f.read(header_size)
            header_json = self.decompressor.decompress(compressed_header)
            header = json.loads(header_json.decode('utf-8'))
            
            # Decompress Tensors
            tensors = {}
            
            # --- FIX STARTS HERE ---
            # Ignore both metadata AND the original header string
            ignored_keys = {'__metadata__', '__original_header__'}
            tensor_names = [k for k in header.keys() if k not in ignored_keys]
            # --- FIX ENDS HERE ---
            
            for name in tensor_names:
                info = header[name]
                compressed_size = info['compressed_size']
                dtype = info['dtype']
                
                # Read & Decompress
                compressed_data = f.read(compressed_size)
                shuffled_data = self.decompressor.decompress(compressed_data)
                
                # Unshuffle (Restore original byte order)
                original_data = self._unshuffle_bytes(shuffled_data, dtype)
                
                tensors[name] = {
                    'data': original_data,
                    'dtype': dtype,
                    'shape': info['shape']
                }

        print(f"Reconstructing safetensors to {output_path}...")

        # If the compressor saved the original header bytes, use them for bit-exact reconstruction.
        original_header_str = header.get('__original_header__')
        
        if original_header_str is not None:
            original_header_bytes = original_header_str.encode('utf-8')

            self._write_safetensors_raw(output_path, original_header_bytes, tensors)
        else:
            self._write_safetensors(output_path, tensors, header.get('__metadata__'))

        decompression_time = time.time() - start_time
        print(f"Decompression finished in {decompression_time:.2f} s")

        return {'time': decompression_time}

    
    def _write_safetensors(self, output_path: str, tensors: Dict, metadata=None):
        """
        Write standard Safetensors format.
        """
        header = {}
        if metadata:
            header['__metadata__'] = metadata
            
        offset = 0
        # Calculate offsets first
        for name, info in tensors.items():
            data_len = len(info['data'])
            header[name] = {
                'dtype': info['dtype'],
                'shape': info['shape'],
                'data_offsets': [offset, offset + data_len]
            }
            offset += data_len
            
        with open(output_path, 'wb') as f:

            header_json = json.dumps(header, separators=(',', ':')).encode('utf-8')
            # 8 bytes for header length (Q)
            f.write(struct.pack('<Q', len(header_json)))
            f.write(header_json)
            
            # Tensor Data
            for name in tensors:
                f.write(tensors[name]['data'])

    def _write_safetensors_raw(self, output_path: str, header_bytes: bytes, tensors: Dict):
        """
        Reconstruct the file using the EXACT original header bytes to ensure
        bit-exact verification passes.
        """
        header = json.loads(header_bytes.decode('utf-8'))
        
        tensor_order = []
        for name, info in header.items():
            if name == '__metadata__': 
                continue
            start_offset = info['data_offsets'][0]
            tensor_order.append((start_offset, name))
        
        # Sort by offset to reconstruct the continuous data block correctly
        tensor_order.sort(key=lambda x: x[0])
        
        with open(output_path, 'wb') as f:
            f.write(struct.pack('<Q', len(header_bytes)))
            
            f.write(header_bytes)
            
            for _, name in tensor_order:
                f.write(tensors[name]['data'])

def verify_compression(original_path, decompressed_path):

    print("=====VERIFICATION=====")
    
    original_size = Path(original_path).stat().st_size
    decompressed_size = Path(decompressed_path).stat().st_size
    
    print(f"Original file:     {original_path}")
    print(f"  Size: {original_size:,} bytes ({original_size/(1024**2):.2f} MB)")
    print(f"Decompressed file: {decompressed_path}")
    print(f"  Size: {decompressed_size:,} bytes ({decompressed_size/(1024**2):.2f} MB)")
    
    if original_size != decompressed_size:
        print(f"\n VERIFICATION FAILED: File sizes differ!")
        print(f"   Difference: {abs(original_size - decompressed_size):,} bytes")
        return False
    
    print(f"\n File sizes match")
    print(f"Performing byte-by-byte comparison...")
    
    # Compare files
    chunk_size = 1024 * 1024  
    total_bytes = original_size
    bytes_compared = 0
    
    with open(original_path, 'rb') as f1, open(decompressed_path, 'rb') as f2:
        while True:
            chunk1 = f1.read(chunk_size)
            chunk2 = f2.read(chunk_size)
            
            if not chunk1 and not chunk2:
                break  
            
            if chunk1 != chunk2:
                print(f"\n VERIFICATION FAILED: Byte mismatch at position {bytes_compared:,}")

                return False
            
            bytes_compared += len(chunk1)
            

            if total_bytes > 10 * 1024 * 1024:  # only show for files > 10 MB
                progress = (bytes_compared / total_bytes) * 100
                print(f"  Progress: {progress:.1f}%", end='\r')
    

    print("VERIFICATION PASSED: Files are identical!")

    
    return True

def main():
    parser = argparse.ArgumentParser(description='Optimized Safetensors Compressor (Bitshuffle+Zstd)')
    parser.add_argument('mode', choices=['compress', 'decompress', 'verify'], help='Mode: compress, decompress, or verify')
    parser.add_argument('input', help='Input file')
    parser.add_argument('output', help='Output file (for compress/decompress) or second file to compare (for verify)')
    parser.add_argument('--level', type=int, default=3, help='Zstd level (Default 3 is optimal for shuffled floats)')
    parser.add_argument('--verify', action='store_true', help='Verify after decompression (requires original file path)')
    parser.add_argument('--original', help='Path to original file for verification after decompression')
    
    args = parser.parse_args()
    
    compressor = SafetensorsCompressor(compression_level=args.level)
    
    if args.mode == 'compress':
        compressor.compress_file(args.input, args.output)
    elif args.mode == 'decompress':
        compressor.decompress_file(args.input, args.output)
        
        # Auto-verify if requested
        if args.verify:
            if not args.original:
                print("\nWarning: --verify requires --original <file> to specify the original file")
            else:
                verify_compression(args.original, args.output)
    elif args.mode == 'verify':
        verify_compression(args.input, args.output)

if __name__ == '__main__':
    main()