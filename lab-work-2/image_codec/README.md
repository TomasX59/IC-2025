# README


## Prerequisites

- Ubuntu/Linux environment
- OpenCV development libraries installed:

```bash
sudo apt update
sudo apt install libopencv-dev
``` 
## How To Test It

### Compile
```bash
make
``` 

### Encode
```bash
./image_codec encode ../imagens/HB7HgEK9.jpeg ../imagens/output.gimg 4096
``` 

#### Output
*Encoding ../imagens/HB7HgEK9.jpeg (735x493), blocksize=4096*

*Wrote ../imagens/output.gimg*


### Decode
```bash
./image_codec decode ../imagens/output.gimg ../imagens/decodedimg.png
``` 
#### Output
*Decoding ../imagens/output.gimg*

*Wrote decodedimg.png*