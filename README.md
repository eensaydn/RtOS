# RtOS - Kooperatif Gorev Zamanlayici

Saf C ile yazilmis, mikro-cekirdek tarzi kooperatif gorev zamanlayici. Tek cekirdekte round-robin zamanlamayla calisir.

> [English version (README-EN.md)](README-EN.md)

## Mimari

```
include/
├── config.h           # Yapilandirma sabitleri (MAX_TASKS, yigin boyutu)
├── task.h             # Gorev yapisi (TCB) ve gorev yonetim API'si
└── scheduler.h        # Zamanlayici genel API'si

src/
├── scheduler.c        # Zamanlayici cekirdegi: init, ana dongu, yield, kill
├── task.c             # Gorev olusturma/yok etme (statik havuzdan)
└── context_switch.c   # Alt seviye baglam kaydetme/geri yukleme (platforma ozel)

examples/
└── main.c             # 3 gorevli demo (round-robin davranisi gosterir)
```

### Nasil Calisiyor

1. **Statik gorev havuzu** -- tum gorevler sabit boyutlu bir dizide (`MAX_TASKS` slot) onceden tahsis edilir. Hicbir yerde heap ayirma (`malloc`) kullanilmaz.

2. **Goreve ozel calisma baglami** -- her gorev kendi yigini (stack) ve CPU durumuna sahiptir. Windows'ta bu, Windows Fibers API'si (`CreateFiber`/`SwitchToFiber`) ile saglanir. Fiber'lar isletim sistemi tarafindan yonetilen yiginlar ve dogru istisna isleme destegi sunar.

3. **Kooperatif yield** -- gorevler `scheduler_yield()` cagirarak kontrolu gonullu olarak zamanlayiciya geri verir. Onceliklendirme (preemption) yoktur; yield yapmayan bir gorev diger tum gorevleri acilikta birakir.

4. **Round-robin dongusu** -- `scheduler_run()` tum gorevleri sirayla tarar. Her `READY` durumdaki gorev icin o gorevin baglamina gecer. Gorev yield yaptiginda kontrol zamanlayiciya doner ve siradaki goreve gecilir.

5. **Baglam degistirme** -- `src/context_switch.c` dosyasinda soyutlanmistir. Windows'ta Fibers API'si kullanilir. `setjmp`/`longjmp`'in neden Windows x86_64'te calismadigi (SEH unwinding yigin cercevelerine yeniden girisi engelliyor) ve Fiber tabanli yaklasimin bunu nasil cozdugu detayli yorumlarla aciklanmistir.

### Baglam Degistirme Diyagrami

```
scheduler_run()                   gorev fonksiyonu
================                  ==================
context_switch_to(gorev) -------> [gorev calisir]
                                   scheduler_yield()
                                     context_switch_to_scheduler()
<---- [zamanlayici devam eder]
[siradaki READY gorevi sec]
context_switch_to(sonraki) -----> [sonraki gorev devam eder]
                                   ...
```

### Neden setjmp/longjmp Yerine Fiber?

Orijinal tasarim, goreve ozel yiginlar icin `setjmp`/`longjmp` ve dogrudan `jmp_buf` manipulasyonu kullaniyordu. Ancak Windows x86_64'te MinGW'nin `longjmp`'i SEH (Structured Exception Handling) unwinding islemini `RtlUnwindEx` uzerinden gerceklestirir. Bu su anlama gelir:

- Cagri yigininda **yukari** `longjmp` yapabilirsiniz (bir ust fonksiyonun `setjmp`'ine)
- Daha once ciktiginiz bir fonksiyona **geri giremezsiniz**

Bu "yeniden giris yok" kisitlamasi, zamanlayicinin gorev fonksiyonlarina tekrar tekrar girip cikmasi gereken kooperatif zamanlama icin `setjmp`/`longjmp`'i kullanilamaz kilar. Windows Fibers tam olarak bu kullanim icin isletim sistemi tarafindan saglanan cozumdur.

POSIX platformlarinda (Linux/macOS) bunun yerine `setjmp`/`longjmp` veya `ucontext` kullanilabilir. Tasima notlari icin `context_switch.c` dosyasina bakin.

## Derleme ve Calistirma

### Gereksinimler

- GCC (Windows'ta MinGW-w64)
- GNU Make (Windows'ta `mingw32-make`)

Windows'ta Chocolatey ile kurulum: `choco install mingw -y`

### Derleme

```bash
# Debug derleme (varsayilan)
mingw32-make

# Release derleme (optimize)
mingw32-make release

# Derleme dosyalarini temizle
mingw32-make clean
```

### Calistirma

```bash
mingw32-make run
# veya dogrudan:
./build/rtos_demo.exe
```

### Beklenen Cikti

```
=== RtOS Cooperative Scheduler Demo ===

[Scheduler] Created task 'Task A' (id=0)
[Scheduler] Created task 'Task B' (id=1)
[Scheduler] Created task 'Task C' (id=2)

[Scheduler] Starting run loop with 3 task(s).
  [Task A] step 1
  [Task B] step 1
  [Task C] step 1
  [Task A] step 2
  [Task B] step 2
  [Task C] step 2
  ...
  [Task C] step 5
  [Task A] step 6
  [Task B] step 6
[Scheduler] Task 'Task C' (id=2) terminated.
  [Task A] step 7
  [Task B] step 7
  ...
  [Task A] step 10
  [Task B] step 10
[Scheduler] Task 'Task A' (id=0) terminated.
[Scheduler] Task 'Task B' (id=1) terminated.
[Scheduler] All tasks completed. Exiting run loop.

=== Demo complete ===
```

## Yapilandirma

`include/config.h` dosyasini duzenleyerek ayarlayabilirsiniz:

| Sabit | Varsayilan | Aciklama |
|-------|-----------|----------|
| `MAX_TASKS` | 8 | Maksimum es zamanli gorev sayisi |
| `DEFAULT_STACK_SIZE` | 8192 | Gorev basina yigin boyutu (byte) |
| `TASK_NAME_MAX` | 32 | Maksimum gorev adi uzunlugu |

## Kisitlamalar

- **Onceliklendirme yok** -- gorevler kooperatif olarak yield yapmali. Takilmis bir gorev her seyi bloklar.
- **Sadece Windows (su an icin)** -- Windows Fibers API'si kullanir. POSIX destegi (`ucontext` veya `setjmp`/`longjmp` ile) planlanmis ama henuz uygulanmamistir. Tasima notlari icin `context_switch.c` dosyasina bakin.
- **Sadece tek cekirdek** -- SMP destegi yok; bu bir kooperatif zamanlayicidir, isletim sistemi cekirdegi degil.
