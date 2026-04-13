# RtOS - Yol Haritasi

> Bu dosya Claude Code icin baglam dosyasidir. Yeni oturum acildiginda
> once bu dosyayi oku -- projenin nerede oldugunu ve nereye gittigini anla.

---

## Simdiye Kadar Yapilan (v0.1 - Temel)

### Ne Insa Edildi
Sifirdan, bos bir repo uzerinde kooperatif gorev zamanlayici insa edildi.

### Dosya Yapisi
```
include/
  config.h         - Sabitler: MAX_TASKS=8, DEFAULT_STACK_SIZE=8192, TASK_NAME_MAX=32
  task.h           - task_t yapisi (TCB), task_state_t enum, gorev API
  scheduler.h      - Zamanlayici genel API: init, create_task, run, yield, kill

src/
  scheduler.c      - Round-robin ana dongu, trampoline deseni, yield/kill
  task.c           - Statik gorev havuzu (malloc yok), olusturma/yok etme
  context_switch.c - Windows Fibers tabanli baglam degistirme

examples/
  main.c           - 3 gorevli demo (A:10 adim, B:10 adim, C:5 adim)

Makefile           - debug/release/clean/run hedefleri
README.md          - Turkce dokumantasyon
README-EN.md       - Ingilizce dokumantasyon
```

### Mimari Kararlar
1. **Tum bellek statik** -- malloc yok, sabit boyutlu task_pool[MAX_TASKS] dizisi
2. **Windows Fibers** -- setjmp/longjmp ile basladik ama Windows x86_64'te
   MinGW'nin longjmp'i SEH unwinding yaptigi icin yigin cercevelerine yeniden
   giris mumkun degil. Fiber'lara gecildi. POSIX tarafinda setjmp/longjmp veya
   ucontext kullanilabilir (henuz yazilmadi).
3. **Trampoline deseni** -- gorev fonksiyonu dogrudan cagrilmaz, task_trampoline()
   sarmalayicisi icinden cagrilir. Fonksiyon dondugunde gorev otomatik TERMINATED olur.
4. **Saf C (C11)** -- dis bagimliliksiz, gcc -Wall -Wextra -pedantic ile sifir uyari

### Derleme Ortami
- Windows 11, Git Bash (MSYS2/MinGW64)
- GCC 15.2.0 (MinGW-w64, Chocolatey ile kuruldu)
- mingw32-make
- PATH ayari ~/.bashrc'ye eklendi: /c/ProgramData/mingw64/mingw64/bin

---

## Gelecek Plani

### Faz 1 - Cekirdek Guclendirme

#### 1.1 Oncelik Tabanli Zamanlama (Priority Scheduling)
- task_t icinde priority alani zaten var ama kullanilmiyor
- Round-robin'i oncelik tabanli yap: yuksek oncelikli gorevler daha sik calissin
- Starvation onleme: dusuk oncelikli gorevler N tur atlanirsa oncelikleri gecici yukselsin
- `scheduler_set_priority(task_id, priority)` API'si ekle

#### 1.2 Gorevler Arasi Iletisim (IPC)
- **Mesaj kuyrugu (message queue)**: sabit boyutlu ring buffer, gorevler arasi veri aktarimi
- `mqueue_create()`, `mqueue_send()`, `mqueue_receive()`
- Kuyruk doluysa/bossa gondericuyi/aliciyi BLOCKED yaparak beklet

#### 1.3 Semafor ve Mutex
- Binary semafor: gorevler arasi senkronizasyon
- Counting semafor: kaynak havuzu yonetimi
- `semaphore_create(initial_count)`, `semaphore_wait()`, `semaphore_signal()`
- BLOCKED durumu artik gercekten kullanilacak

#### 1.4 Zamanlayici (Timer) Destegi
- Yazilimsal tick sayaci: her yield'de tick artir
- `scheduler_sleep(ticks)` -- gorev belirli tick sayisi kadar beklesin
- Timeout mekanizmasi: semafor/kuyruk bekleme icin zaman asimi

### Faz 2 - Gozlemlenebilirlik ve Hata Ayiklama

#### 2.1 Gorev Istatistikleri
- Her gorev icin: toplam calisma suresi, yield sayisi, baglam degisimi sayisi
- `scheduler_get_stats(task_id)` API'si
- Istenal olarak terminale ozet tablo yazdirma

#### 2.2 Yigin Tasmasi Koruması
- Her gorevin yigininin basina/sonuna "canary" degeri yaz (ornegin 0xDEADBEEF)
- Her baglam degisiminde canary degerini kontrol et
- Tasma tespit edilirse gorev sonlandirilsin ve hata basilsin

#### 2.3 Loglama Altyapisi
- LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR seviyeleri
- Derleme zamaninda seviye filtreleme (config.h'de LOG_LEVEL)
- Zaman damgasi (tick bazli) ile log ciktisi

### Faz 3 - Ileri Ozellikler

#### 3.1 Dinamik Gorev Olusturma/Silme
- Zamanlayici calisirken yeni gorev olusturabilme
- Biten gorevlerin slotlarini geri donusturme (su an slot yeniden kullanimi sinirli)

#### 3.2 Olay Tabanli Uyandirma (Event Flags)
- Gorevler belirli olaylari bekleyebilsin
- `event_wait(flags)`, `event_set(flags)`, `event_clear(flags)`
- Birden fazla gorev ayni olayi bekleyebilir

#### 3.3 POSIX Portu
- context_switch.c icine Linux/macOS destegi ekle
- ucontext (getcontext/makecontext/swapcontext) tabanli implementasyon
- #ifdef _WIN32 / #else dallanmasiyla ayni API, farkli arka uc

#### 3.4 Bellek Havuzu (Memory Pool)
- Sabit boyutlu blok ayirici: malloc olmadan dinamik-benzeri ayirma
- `mempool_create(block_size, block_count)`, `mempool_alloc()`, `mempool_free()`
- Gorev yiginlari icin kullanilabilir

### Faz 4 - Gercek Dunya Entegrasyonu

#### 4.1 UART/Seri Port Simulasyonu
- Basit bir I/O surucusu: gorev bazli seri port okuma/yazma
- Kesme (interrupt) simulasyonu ile gorev uyandirma

#### 4.2 Shell / Komut Satiri
- Basit bir komut yorumlayicisi gorevi
- `ps` -- gorev listesi, `kill <id>` -- gorev sonlandirma, `stats` -- istatistikler
- Stdin'den okuyup komut calistirma

#### 4.3 Test Framework
- Her modul icin birim testleri
- Stres testi: MAX_TASKS gorev olusturup hepsini calistir
- Bellek sizintisi/tasma testleri (canary kontrolu)

---

## Gelistirme Sirasi Onerisi

Yukaridaki fazlar sirasiz degil, birbiri uzerine insa eder:

```
v0.1 [TAMAM] Temel zamanlayici + round-robin + demo
     |
v0.2 Oncelik zamanlama + sleep/timer
     |
v0.3 Semafor + mutex + BLOCKED durumu aktif
     |
v0.4 Mesaj kuyrugu (IPC)
     |
v0.5 Yigin korumasi + istatistikler + loglama
     |
v0.6 Olay bayraklari + dinamik gorev yonetimi
     |
v0.7 POSIX portu
     |
v0.8 Shell + test framework
     |
v1.0 Kararli surum
```

---

## Notlar

- Her yeni ozellik icin once `examples/` altina demo ekle
- Mevcut API'yi bozmadan genislet (geriye uyumluluk)
- Her faz sonunda `mingw32-make` ile sifir uyari dogrula
- Bu dosyayi her fazdan sonra guncelle: yapilani [TAMAM] olarak isaretle
