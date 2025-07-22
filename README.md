This is the code repository of LogFold. 


# Dependencies
please make sure your environment statisfies all the following depdencies:


cmake >= 3.20

gcc >= 12

PCRE2 == 10.42

tar == 1.30

python >= 3.8


# Public Log Dataset
We use the public log dataset from [LogHub1.0](https://github.com/logpai/loghub).
Please download the datasets and put them in the `Logs` directory.
```
cd LogFold && mkdir Logs
```

# Compresion
## Compile 
### 1. creat a new ```build``` directory
```
$ cd LogFold && mkdir build
```

### 2. enter the ```build``` directory and exectue the cmake command.
```
$ cd build && cmake ..
```

### 3. compile the source code.
```
$ make -j
```


## Exectuion
When the complitation is done, you will get the executable binary named as ```LogFold```.
To start the compression, plese use the following commandline:
```
./LogFold xxxxx.log -o xxx-output
```
For more details about the args, please use:
```
./LogFold -h
```

# Decompression
We provide python scripts for decompression. 
There are serverl steps to restore the original logs from the compressed ones.
## 1. decompress the main archive
```
$ mkdir decompress
$ tar -xvf xxx.tar.xz -C decompress
```
## 2. decompress the IDs of static token sequences
```
python3 decompress_pattern_ids.py templateid.bin
```
## 3. restore the original order of static token sequences
```
python3 restore_pattern_templates.py template.txt decompressed-pattern-ids.txt
```

## 4.decompress the IDs of unstructured strings
```
python3 decompress_dynamic_ids.py tokenid.bin
```
## 5. replaces the ```<*>``` placeholders with corresponding IDs
```
python3 replace_placeholder_with_id.py template_new.txt decompressed-d-ids.txt
```

## 6. decompress the unstructured numbers
```
python3 decompress_length_binary.py ./
```

## 7. replace the length-based tags with decompressed unstructured numbers
```
python3 replace_length_vars.py template_new_restored.txt ./
```

## 8. final recover 
```
python3 2new_decode_and_decompress_matrix.py template_new_restored_replaced.txt ./ final.out
```
The `final.out` is the decompressed logs.
