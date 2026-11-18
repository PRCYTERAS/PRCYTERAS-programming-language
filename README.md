# PTS Beta 1.0

PTS (PRCYTERAS) is a programming language and compiler for native Windows x64 programs. The `ornekler` folder contains the included `.pts` learning examples.

## Türkçe

Yeni bir CMD veya PowerShell penceresinde derleyin:

```powershell
pts cargo "C:\Projeler\program.pts"
```

Release için `--release`, farklı x64 işlemcilerle daha uyumlu release çıktısı için `--release --portable` kullanın. Kurulumdaki `ornekler` klasöründe 88 adet `.pts` örneği bulunur. Bu klasördeki `PTS-README-TR.txt` Türkçe başlangıç kılavuzudur.

Release optimizasyonları hız artışı garanti etmez. Signed tamsayı taşması tanımsızdır; fast-math kayan nokta sonuçlarını değiştirebilir. C++ runtime, IEEE kayan nokta davranışını korumak için fast-math olmadan derlenir. Derleyici hataları kaynak satırını ve konumunu gösterir.

## English

Open a new Command Prompt or PowerShell window and build:

```powershell
pts cargo "C:\Projects\program.pts"
```

Use `--release` for release builds, or `--release --portable` for release executables more suitable for different x64 processors. The installation's `ornekler` folder contains 88 `.pts` examples. `PTS-README-EN.txt` in this folder is the English quick-start guide.

Release optimizations do not guarantee a speedup. Signed integer overflow is undefined, and fast-math can change floating-point results. The C++ runtime is compiled without fast-math to preserve IEEE floating-point behavior. Compiler diagnostics show the source line and location.
