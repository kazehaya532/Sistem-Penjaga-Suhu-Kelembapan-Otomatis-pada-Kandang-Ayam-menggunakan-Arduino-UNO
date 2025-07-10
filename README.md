# Sistem-Penjaga-Suhu-Kelembapan-Otomatis-pada-Kandang-Ayam-menggunakan-Arduino-UNO
---
### **1. Setup Awal**
* **Mengimpor Library**: Menggunakan library untuk sensor DHT (suhu/kelembapan), servo, dan LCD I2C.
* **Definisi Pin**: Menetapkan pin-pin Arduino untuk sensor DHT, motor kipas, servo, potensiometer (untuk usia ayam), dan relay lampu.
* **Inisialisasi Objek**: Membuat objek untuk sensor DHT, servo, dan LCD.

---

### **2. Fungsi Fuzzy (Sugeno FIS)**
* **Fungsi Keanggotaan**: Mendefinisikan fungsi `fuzzyUp`, `fuzzyDown`, dan `fuzzyTri` untuk mengkonversi nilai sensor (suhu, kelembapan, usia) menjadi derajat keanggotaan fuzzy (misalnya, seberapa 'panas' suatu suhu).
* **`sugenoFIS` Function**: Ini adalah inti dari sistem.
    * **Fuzzifikasi**: Mengubah input (suhu, kelembapan, usia) menjadi nilai-nilai fuzzy (dingin, normal, panas; kering, ideal, basah; bayi, muda, remaja, tua).
    * **Aturan Fuzzy**: Memiliki 36 aturan yang menggabungkan semua kemungkinan kondisi fuzzy dari suhu, kelembapan, dan usia. Setiap aturan menghasilkan nilai alpha (kekuatan aturan) serta nilai PWM kipas dan sudut servo yang direkomendasikan.
    * **Defuzzifikasi**: Menggabungkan hasil dari semua aturan untuk menghitung nilai akhir PWM kipas dan sudut servo yang optimal.

---

### **3. Kontrol Perangkat Keras**
* **`setup()`**:
    * Menginisialisasi komunikasi serial, sensor DHT, servo, dan LCD.
    * Menetapkan mode pin sebagai output.
    * Mengatur arah putaran kipas awal dan mematikan lampu.
* **`loop()`**:
    * **Membaca Sensor**: Membaca suhu dan kelembapan dari sensor DHT.
    * **Membaca Usia Ayam**: Membaca nilai dari potensiometer dan memetakannya ke rentang usia ayam (1-30 hari).
    * **Kontrol Lampu**: Menghitung suhu target berdasarkan usia ayam dan mengaktifkan/menonaktifkan lampu menggunakan logika histeresis untuk menghindari fluktuasi cepat.
    * **Mengaplikasikan FIS Sugeno**: Memanggil fungsi `sugenoFIS` dengan data suhu, kelembapan, dan usia untuk mendapatkan nilai PWM kipas dan sudut servo yang direkomendasikan.
    * **Mengontrol Aktuator**: Mengatur kecepatan kipas (`analogWrite`) dan posisi servo ventilasi (`ventilasi.write`) sesuai dengan output dari FIS.

---

### **4. Tampilan dan Debugging**
* **Serial Monitor**: Mencetak semua data penting (usia, suhu target, suhu aktual, kelembapan, PWM kipas, sudut ventilasi, status lampu) ke Serial Monitor untuk debugging dan pemantauan.
* **LCD Display**: Menampilkan informasi secara bergantian di layar LCD setiap 3 detik. Halaman pertama menunjukkan usia, suhu, dan kelembapan. Halaman kedua menunjukkan persentase kipas, status lampu, dan sudut servo.
