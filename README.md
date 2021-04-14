# An Integrated Memory and SSD Caches Management Scheme (一個整合記憶體快取和SSD快取的管理機制)

此專題為我大學時期的畢業專題，由張軒彬教授指導，和同學吳哲宇合力完成。

本專題提出了新的系統架構，將快取緩衝器(page cache)和固態硬碟快取(SSD cache)的管理機制整合為一，解決了傳統SSD based disk cache會產生的問題，並且讓兩者互相合作，進一步提升了多層級儲存裝置的效能。

下方會簡述本專題的動機與解決方法，詳細內容請參閱pdf檔。。

## 舊的架構下所產生的問題

### 1. Metadata Duplication

為了能在快取空間滿的時候，挑選適當的資料逐出，許多快取演算法都會維護對應的metadata，因此，某些block的metadata會同時被page cache和SSD cache的替換演算法所維護，造成重複維護的現象。

### 2. Double Caching

因為page cache和SSD cache各自獨立運作，在某些情形下，資料可能同時被快取於page cache以及SSD cache。因為快取空間有限，重複被快取將會浪費寶貴的快取空間

### 3. Inaccurate/delayed Information

如上述，許多SSD cache的替換演算法會維護block的metadata，但是，只有當某一block被逐出page cache時，其metadata才會有機會被SSD cache取得並維護。也就是說，當一個block在page cache被存取多次時，SSD cache並無法得知此資訊，並且，只有在某個block被逐出page cache時，SSD cache才有機會開始對其維護metadata，造成SSD cache取得的資訊Inaccurate也delayed。

### 4. Blind page cache management

在現行的架構下，page cache可能會同時快取來自HDD與SSD的資料。但是，由於page cache和SSD cache兩者各自獨立，page cache無法得知哪些資料來自HDD，哪些來自SSD，使得page cache無法針對來自不同裝置的資料實施不同的管理策略。

## 我們提出的方法

### 1. 新的架構

我們將SSD cache的管理模組上移，使page cache能知道SSD cache的存在，並且將page cache的空間分為兩部分，page cache_SSD快取來自SSD的資料，而page cache_HDD快取來自HDD的資料。

在這樣的架構下，我們解決了上面提到的Blind page cache management的問題，再來，也可以動態調整page cache_SSD和page cache_HDD兩者的空間，來更聰明地選擇victim block。

### 2. 整合Metadata

為了解決metadata重複維護的問題，我們設計了整合的資料結構，將page cache和SSD cache所需要的metadata統一管理，並讓page cache與SSD cache都能存取。如此一來，就能避免metadata duplication，並且，因為SSD cache可以存取所有的metadata，上面所提的Inaccurate/delayed Information的問題也能避免了。

### 3. 減少Double Caching

因為page cache也會快取來自SSD cache的資料，double caching是不可避免的，但是，我們藉由修改page cache的管理策略來減少此一現象發生。

### 4. Page cache_SSD與page cache_HDD的動態分割

為了能適當分配兩者的空間以得到最大效能，我們會週期性的預測下一週期的request並分配空間，使HDD和SSD可以load balance。
