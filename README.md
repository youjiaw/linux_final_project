# linux_final_project
詳細實作紀錄: https://hackmd.io/@sysprog/B1TOj7mwR

## 性能比較
| 指標                        | 原程式碼結果     | 最終程式碼結果   | 改善幅度   |
|-----------------------------|-----------------|-----------------|-----------|
| cache-misses (cpu_core)     | 33,1258 次     | 7,5328 次       | +78%    |
| cache-misses (cpu_atom)     | 12,8966 次      | 1,9366 次       | +85%    |
| cache-references (cpu_core) | 172,6061 次     | 137,6053 次     | +20%    |
| cache-references (cpu_atom) | 48,5840 次     | 9,2884 次       | +80%    |
| L1-dcache-load-misses       | 51,5655 次      | 29,2287 次      | +43%    |
| L1-icache-load-misses (cpu_core) | 93,9572 次      | 54,6282 次      | +41%    |
| L1-icache-load-misses (cpu_atom) | 25,0156 次      | 12,1926 次      | +51%    |
| LLC-load-misses (cpu_core)  | 7717 次         | 1072 次         | +86%    |
| LLC-load-misses (cpu_atom)  | 1235 次         | 0 次            | +100%     |
| page-faults                 | 8208 次         | 1646 次         | +80%    |
| context-switches            | 39 次           | 55 次           | -41%    |
| cpu-migrations              | 25 次           | 15 次           | +40%    |
| task-clock                  | 115.84 msec     | 108.55 msec     | +6%     |
| 執行時間                    | 0.017522 秒     | 0.023473 秒     | -34%    |

修改後的排序方法可以優化快取的利用率，讓資料還在快取上層時再次被使用。
