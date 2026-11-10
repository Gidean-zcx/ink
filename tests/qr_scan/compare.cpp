#include <ReadBarcode.h>
#include <ReaderOptions.h>
#include <ImageView.h>
#include <quirc.h>
#include <quirc_internal.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <vector>
int main(int argc, char **argv) {
 for (int i=1;i<argc;i++) {
  std::ifstream f(argv[i],std::ios::binary); std::vector<unsigned char> p(640*480); f.read((char*)p.data(),p.size()); if(!f)return 2;
  auto q=quirc_new(); if(!q || quirc_resize(q,640,480))return 3;
  auto start=std::chrono::steady_clock::now(); std::copy(p.begin(),p.end(),quirc_begin(q,nullptr,nullptr)); quirc_end(q);
  quirc_code code{}; quirc_data data{}; bool ok=false;
  for(int n=0;n<quirc_count(q);n++){quirc_extract(q,n,&code); if(!quirc_decode(&code,&data)){ok=true;break;}}
  auto end=std::chrono::steady_clock::now();
  auto zstart=end; auto z=ZXing::ReadBarcode({p.data(),640,480,ZXing::ImageFormat::Lum},ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode));
  auto zend=std::chrono::steady_clock::now();
  printf("%s quirc=%d us=%lld zxing=%d us=%lld\n",argv[i],ok,(long long)std::chrono::duration_cast<std::chrono::microseconds>(end-start).count(),z.isValid(),(long long)std::chrono::duration_cast<std::chrono::microseconds>(zend-zstart).count());
  printf("quirc heap=%zu decode structs=%zu\n",sizeof(*q)+640*480+320*sizeof(quirc_flood_fill_vars),sizeof(code)+sizeof(data));quirc_destroy(q);
 }
}
