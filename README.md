# PTS Beta 1.0
# 🚀 PRCYTERAS Beta 1.0

### ⬇️ [Kurulum Sihirbazını Doğrudan İndir (PTS-beta1.0-download.exe)](https://github.com/PRCYTERAS/PRCYTERAS-programming-language/releases/tag/v1.0.0-beta)
> *kurulum sihirbazını tek tıkla indirip kurun*

PTS (PRCYTERAS) is a programming language and compiler for native Windows x64 programs. The `ornekler` folder contains the included `.pts` learning examples.

## Türkçe

Yeni bir CMD veya PowerShell penceresinde derleyin:

```powershell
(pts cargo (dosya adı).pts --release --windowed --portable) pts cargo (dosya adı).pts (release ileri düzey agresif performans sağlar --windowed arkada cmd ekranının açılmamasını sağlar oyunlar ve programlar için ideal ve --portable ise --release çıktısını farklı x64 işlemcilerde çalışmaya daha uygun üretir ve üçüde pts cargo (dosya adı).pts --windowed --release --portable olarak kullanılabilir 
```

Release için `--release`, farklı x64 işlemcilerle daha uyumlu release çıktısı için `--release --portable` kullanın. Kurulumdaki `ornekler` klasöründe 88 adet `.pts` örneği bulunur. Bu klasördeki `PTS-README-TR.txt` Türkçe başlangıç kılavuzudur.

Release optimizasyonları hız artışı garanti etmez. Signed tamsayı taşması tanımsızdır; fast-math kayan nokta sonuçlarını değiştirebilir. C++ runtime, IEEE kayan nokta davranışını korumak için fast-math olmadan derlenir. Derleyici hataları kaynak satırını ve konumunu gösterir.

## English

Open a new Command Prompt or PowerShell window and build:

```powershell
(pts cargo (file name).pts --release --windowed --portable pts cargo (file_name).pts — The 'release' option provides an advanced, high-performance optimization; the '--windowed' option prevents a command prompt (CMD) window from appearing in the background, making it ideal for games and applications. The '--portable' option ensures that the 'release' output is better suited for execution across a variety of x64 processors and all three can be used as: pts cargo (file name).pts --windowed --release --portable
```

Use `--release` for release builds, or `--release --portable` for release executables more suitable for different x64 processors. The installation's `ornekler` folder contains 88 `.pts` examples. `PTS-README-EN.txt` in this folder is the English quick-start guide.

Release optimizations do not guarantee a speedup. Signed integer overflow is undefined, and fast-math can change floating-point results. The C++ runtime is compiled without fast-math to preserve IEEE floating-point behavior. Compiler diagnostics show the source line and location.
