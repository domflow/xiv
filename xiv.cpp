#include <bits/stdc++.h>
#include <zlib.h>
using namespace std;

constexpr size_t BS = 4096, MAP = BS / 8;

class Progress {
    size_t total;
    chrono::steady_clock::time_point start;
public:
    Progress(size_t t): total(t), start(chrono::steady_clock::now()) {}
    void update(size_t done) {
        int w = 40;
        double r = (double)done / total;
        size_t f = (size_t)(r * w);
        auto now = chrono::steady_clock::now();
        double el = chrono::duration<double>(now - start).count();
        double eta = done ? el / done * (total - done) : 0;
        cout << "\r[";
        for (size_t i = 0; i < w; ++i) cout << (i < f ? '#' : ' ');
        cout << "] " << setw(3) << int(r * 100)
             << "% | ETA: " << fixed << setprecision(1) << eta << "s" << flush;
        if (done == total) cout << "\n";
    }
};

vector<uint8_t> enc(const uint8_t *blk) {
    vector<uint8_t> o;
    bool seen[256] = {};
    for (size_t i = 0; i < BS; i++) seen[blk[i]] = 1;
    for (int v = 0; v < 256; v++) if (seen[v]) {
        o.push_back(v);
        uint8_t bm[MAP] = {};
#ifdef USE_ASM
        for (size_t i = 0; i < BS; i++) if (blk[i] == v) {
            size_t b = i / 8; int bit = 7 - (i % 8);
            asm volatile("orb %1,(%0)"::"r"(bm + b),"r"((uint8_t)(1 << bit)):"memory");
        }
#else
        for (size_t i = 0; i < BS; i++) if (blk[i] == v)
            bm[i / 8] |= (1 << (7 - i % 8));
#endif
        o.insert(o.end(), bm, bm + MAP);
    }
    return o;
}

vector<uint8_t> dec(const vector<uint8_t>& d) {
    vector<uint8_t> o(BS);
    for (size_t i = 0; i + 1 + MAP <= d.size(); i += 1 + MAP) {
        uint8_t v = d[i];
        for (size_t j = 0; j < MAP; j++)
            for (int b = 0; b < 8; b++)
                if (d[i + 1 + j] & (1 << (7 - b)))
                    o[j * 8 + b] = v;
    }
    return o;
}

vector<uint8_t> zcomp(const vector<uint8_t>& in) {
    uLongf n = compressBound(in.size());
    vector<uint8_t> o(n);
    if (compress2(o.data(), &n, in.data(), in.size(), Z_BEST_COMPRESSION) != Z_OK)
        throw runtime_error("compress");
    o.resize(n); return o;
}

vector<uint8_t> zdecomp(const vector<uint8_t>& in) {
    uLongf n = BS * 8; vector<uint8_t> o(n);
    int r;
    while ((r = uncompress(o.data(), &n, in.data(), in.size())) == Z_BUF_ERROR)
        o.resize(n *= 2);
    if (r != Z_OK) throw runtime_error("uncompress");
    o.resize(n); return o;
}

void w32(ofstream &f, uint32_t v){for(int i=0;i<4;i++)f.put((v>>(8*i))&255);}
uint32_t r32(ifstream &f){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint8_t)f.get()<<(8*i);return v;}
void w64(ofstream &f,uint64_t v){for(int i=7;i>=0;i--)f.put((v>>(8*i))&255);}
uint64_t r64(ifstream &f){uint64_t v=0;for(int i=7;i>=0;i--)v|=(uint64_t)(uint8_t)f.get()<<(8*i);return v;}

void encode(const string &in,const string &out){
    ifstream fi(in,ios::binary); ofstream fo(out,ios::binary);
    fi.seekg(0,2); uint64_t sz=fi.tellg(); fi.seekg(0);
    w64(fo,sz);
    size_t total = (sz + BS - 1) / BS;
    Progress bar(total);
    vector<uint8_t> blk(BS);
    for (size_t blk_i=0; blk_i<total; ++blk_i) {
        fi.read((char*)blk.data(),BS);
        size_t g=fi.gcount(); if(g<BS) fill(blk.begin()+g,blk.end(),0);
        auto e=enc(blk.data()), c=zcomp(e);
        w32(fo,c.size()); fo.write((char*)c.data(),c.size());
        bar.update(blk_i+1);
    }
}

void decode(const string &in,const string &out){
    ifstream fi(in,ios::binary); ofstream fo(out,ios::binary);
    uint64_t sz=r64(fi); uint64_t written=0;
    vector<uint32_t> chunk_lens;
    streampos pos = fi.tellg();
    while(fi.peek()!=EOF){ try{chunk_lens.push_back(r32(fi));}catch(...){break;}
        fi.seekg(chunk_lens.back(),ios::cur);
    }
    fi.clear(); fi.seekg(pos);
    Progress bar(chunk_lens.size());
    for(size_t blk_i=0; blk_i<chunk_lens.size(); ++blk_i){
        uint32_t n=r32(fi); vector<uint8_t> c(n); fi.read((char*)c.data(),n);
        auto e=zdecomp(c), b=dec(e);
        auto e2=enc(b.data()), c2=zcomp(e2);
        if(c2!=c) throw runtime_error("verify fail");
        size_t w=min<uint64_t>(BS,sz-written);
        fo.write((char*)b.data(),w); written+=w;
        bar.update(blk_i+1);
    }
}

int main(int a,char**v){
    if(a!=4){cerr<<"use: g6_small encode|decode in out\n";return 1;}
    string m=v[1];
    if(m=="encode") encode(v[2],v[3]);
    else decode(v[2],v[3]);
}
