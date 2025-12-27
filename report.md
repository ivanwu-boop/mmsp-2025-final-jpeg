# MMSP 2025 Final Project Report

## 1. Project Overview

本專案依照課程指定之題目（mmsp-2025-final-jpeg），實作一套簡化的 JPEG-like 影像壓縮系統。實作內容涵蓋影像前處理、頻域轉換、量化、資料重排，以及基本的熵編碼方法，目的在於理解 JPEG 壓縮流程中各階段的設計理念與其對影像品質的影響。

本實作並非完整 JPEG bitstream（如 Huffman coding），而是以課程規格為主，聚焦於核心訊號處理步驟之正確性。

---

## 2. JPEG Compression Pipeline

整體流程如下：

1. RGB 色彩空間轉換為 YCbCr（BT.601）
2. Level shift（將像素值移至 -128 ~ 127）
3. 將影像切分為 8×8 block
4. 對每個 block 進行 DCT（Discrete Cosine Transform）
5. 使用標準 JPEG 量化表進行量化
6. 以 Zig-Zag 順序重排係數
7. （Method 3）對 DC 係數進行 DPCM，AC 係數進行 RLE

Decoder 則依相反順序進行 dequantization、IDCT 與影像重建。

---

## 3. Method Description

### 3.1 Method 0 – RGB Channel Extraction

Method 0 為 baseline 實驗，用以驗證 BMP I/O 與影像資料處理正確性。Encoder 將 BMP 影像分離為 R、G、B 三個文字檔，Decoder 再由該三個檔案完整重建影像。此方法為無失真（lossless），重建結果與原始影像完全相同。

---

### 3.2 Method 1 – DCT and Quantization

Method 1 實作 JPEG 的主要壓縮步驟，包括：

* RGB → YCbCr 色彩轉換
* 對 Y、Cb、Cr 三個分量皆進行 level shift
* 8×8 block DCT
* 使用標準 JPEG luminance / chrominance quantization table 進行量化
* Zig-Zag 掃描後輸出量化係數

此方法僅進行編碼，不產生重建影像，主要用於觀察量化後係數分布。

---

### 3.3 Method 2 – Dequantization and IDCT

Method 2 為 Method 1 的對應 decoder，流程包含：

* Inverse Zig-Zag 重建 2D 頻域係數
* Dequantization
* IDCT
* Level shift reversal
* YCbCr → RGB 轉換

最終輸出重建影像，並與原始 BMP 計算 PSNR 以評估影像品質。

---

### 3.4 Method 3 – DPCM and RLE (Entropy Coding)

Method 3 實作簡化的熵編碼流程：

* DC 係數以 Differential PCM（DPCM）方式編碼
* AC 係數以 Run-Length Encoding（RLE）方式表示

依題目規格，本方法僅要求 encoder，未實作 entropy decoder。此設計選擇符合課程要求，並非實作遺漏。

---

## 4. Experimental Results

實驗使用課程提供之測試影像（如 Kimberly.bmp）。

在 Method 2 重建結果中，影像品質與原始影像高度相似，僅在高頻細節處出現輕微失真。PSNR 結果約落在 30–35 dB 範圍，符合使用標準 JPEG 量化表時的預期品質。

---

## 5. Discussion

* DCT 能有效集中能量於低頻係數，使量化後多數高頻係數為零。
* 量化為主要失真來源，直接影響 PSNR 與視覺品質。
* Zig-Zag 掃描有助於提升後續 RLE 的壓縮效率。
* Method 3 的 DPCM + RLE 展示了 JPEG 中熵編碼前處理的核心概念。

---

## 6. Conclusion

本專案成功依照課程規格完成 JPEG-like 影像壓縮系統的實作，並驗證各模組之正確性。透過 PSNR 評估，可確認在合理的壓縮條件下，影像品質仍能維持良好水準。本實作有助於理解 JPEG 壓縮流程中頻域轉換與量化設計對影像品質的影響。
