//final version
#include "stdafx.h"
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <time.h>

#define SSD_READ_RATE 550
#define SSD_WRITE_RATE 500
#define HDD_TRANSFER_RATE 6
#define PERIOD 1000
#define SSD_THRESHOLD 2
#define ALPHA 0.6
#define PAGECACHE_SIZE_RATIO 0.4
#define SSDCACHE_SIZE_RATIO 2


using namespace std;


double get_ssd_time(int amount, bool isWrite) {
	//the return unit is ms
	int temp = amount / 4;
	if (amount % 4 != 0) {
		temp++;
	}
	if (isWrite) {
		return temp*(4.0 * 1000 / (SSD_WRITE_RATE * 1024));
	}
	else {
		return temp*(4.0 * 1000 / (SSD_READ_RATE * 1024));
	}
}
double get_hdd_time(int amount) {
	//the return unit is ms
	// 10 + 4.2 + amount / (6 * 1024 * 1024 / 1000);
	return 10 + 4.2 + amount / (6 * 1024 * 1024 / 1000);
}

class node {
public:
	int blockNum;
	int hitCount;
	bool pageCacheDirty;
	bool ssdCacheDirty;
	bool inPageCacheSSD;
	bool inPageCacheHDD;
	bool inSsdCache;
	node *PCH_next;
	node *PCH_prev;
	node *PCS_next;
	node *PCS_prev;
	node *SC_next;
	node *SC_prev;

	node(int const &n) : blockNum(n) {
		hitCount = 0;
		pageCacheDirty = false;
		ssdCacheDirty = false;
		inPageCacheSSD = false;
		inPageCacheHDD = false;
		inSsdCache = false;
		PCH_next = nullptr;
		PCH_prev = nullptr;
		PCS_next = nullptr;
		PCS_prev = nullptr;
		SC_next = nullptr;
		SC_prev = nullptr;
	}
};

class CList {
protected:
	node *head;
	node *tail;
	int length;
	int capacity;
public:
	virtual void move_to_front(node *target) = 0;
	virtual void push_front(node *target) = 0;
	virtual void pop_back() = 0;
	virtual int isFull() {
		if (length == capacity) {
			return 1;
		}
		else if (length > capacity) {
			return 2;
		}
		else {
			return 0;
		}
	}
	virtual node *back() {
		return tail;
	}
	virtual node *front() {
		return head;
	}
	virtual int len() {
		return length;
	}
	virtual int Capacity() {
		return capacity;
	}
	virtual void Resize(int newcapa) {
		capacity = newcapa;
	}
};

class CPageCacheHdd : public CList {
private:
	int cacheCount;
public:
	CPageCacheHdd(int const &c) {
		head = nullptr;
		tail = nullptr;
		length = 0;
		capacity = c;
	}

	void move_to_front(node *target) {
		if (target == head)	return;

		target->PCH_prev->PCH_next = target->PCH_next;
		if (tail == target)
			tail = target->PCH_prev;
		else
			target->PCH_next->PCH_prev = target->PCH_prev;
		target->PCH_next = head;
		target->PCH_prev = nullptr;
		head->PCH_prev = target;
		head = target;
	}
	void push_front(node *target) {
		target->PCH_next = head;
		if (tail == nullptr)
			tail = target;
		if (head != nullptr)
			head->PCH_prev = target;
		head = target;
		target->inPageCacheHDD = true;
		length++;
	}
	void pop_back() {
		/*lru pop back*/
		node *target = tail;
		target->inPageCacheHDD = false;
		if (head == tail)
			head = nullptr;
		tail = tail->PCH_prev;
		if (tail != nullptr)
			tail->PCH_next = nullptr;
		length--;
		target->PCH_next = nullptr;
		target->PCH_prev = nullptr;
	}
	int number(node *target) {
		int count = 0;
		for (node *temp = head; temp != nullptr; temp = temp->PCH_next, count++) {
			if (temp == target) {
				return count;
			}
		}
		return -1;
	}
	double cacheRatio() {
		int ctr = 0;
		for (node* i = head; i != nullptr; i = i->PCH_next) {
			if (i->hitCount > SSD_THRESHOLD) {
				ctr++;
			}
		}
		return (double)ctr / capacity;
	}
	double dirtyRatio() {
		int ctr = 0;
		for (node* i = head; i != nullptr; i = i->PCH_next) {
			if (i->pageCacheDirty) {
				ctr++;
			}
		}
		return (double)ctr / capacity;
	}
};

class CPageCacheSsd : public CList {
private:
public:
	CPageCacheSsd(int const &c) {
		head = nullptr;
		tail = nullptr;
		length = 0;
		capacity = c;
	}

	void move_to_front(node *target) {
		if (target == head)	return;

		target->PCS_prev->PCS_next = target->PCS_next;
		if (tail == target)
			tail = target->PCS_prev;
		else
			target->PCS_next->PCS_prev = target->PCS_prev;
		target->PCS_next = head;
		target->PCS_prev = nullptr;
		head->PCS_prev = target;
		head = target;
	}
	void push_front(node *target) {
		target->PCS_next = head;
		if (tail == nullptr)
			tail = target;
		if (head != nullptr)
			head->PCS_prev = target;
		head = target;
		target->inPageCacheSSD = true;
		length++;
	}
	void pop_back() {
		//kick it
		node *target = tail;
		target->inPageCacheSSD = false;
		if (head == tail)
			head = nullptr;
		tail = tail->PCS_prev;
		if (tail != nullptr)
			tail->PCS_next = nullptr;
		length--;
		target->PCS_next = nullptr;
		target->PCS_prev = nullptr;
	}
	int number(node *target) {
		int count = 0;
		for (node *temp = head; temp != nullptr; temp = temp->PCS_next, count++) {
			if (temp == target) {
				return count;
			}
		}
		return -1;
	}
	double dirtyRatio() {
		int ctr = 0;
		for (node* i = head; i != nullptr; i = i->PCS_next) {
			if (i->pageCacheDirty) {
				ctr++;
			}
		}
		return (double)ctr / capacity;
	}
};

class CSsdCache : public CList {
public:
	CSsdCache(int const &c) {
		head = nullptr;
		tail = nullptr;
		length = 0;
		capacity = c;
	}

	void move_to_front(node *target) {
		if (target == head)	return;

		target->SC_prev->SC_next = target->SC_next;
		if (tail == target)
			tail = target->SC_prev;
		else
			target->SC_next->SC_prev = target->SC_prev;
		target->SC_next = head;
		target->SC_prev = nullptr;
		head->SC_prev = target;
		head = target;
	}
	void push_front(node *target) {
		/*add to mru of larc list*/
		target->SC_next = head;
		if (tail == nullptr)
			tail = target;
		if (head != nullptr)
			head->SC_prev = target;
		head = target;
		target->inSsdCache = true;
		length++;
	}
	void pop_back() {
		node *target = tail;
		target->inSsdCache = false;
		if (head == tail)
			head = nullptr;
		tail = tail->SC_prev;
		if (tail != nullptr)
			tail->SC_next = nullptr;
		length--;
		target->SC_next = nullptr;
		target->SC_prev = nullptr;
	}
	double dirtyRatio() {
		int ctr = 0;
		for (node* i = head; i != nullptr; i = i->SC_next) {
			if (i->ssdCacheDirty) {
				ctr++;
			}
		}
		return (double)ctr / capacity;
	}
};
//resize
class ResizeInfo {
public:
	//previous
	int PCH_read_prev;
	int PCH_write_prev;
	int PCS_read_prev;
	int PCS_write_prev;
	//
	double PCS_dirtyRatio_prev;
	double PCH_dirtyRatio_prev;
	double SC_dirtyRatio_prev;
	double PCH_cacheRatio_prev;
	//current
	int PCH_read_hit_cur;
	int PCH_read_miss_cur;
	int PCH_write_hit_cur;
	int PCH_write_miss_cur;
	//
	int PCS_read_hit_cur;
	int PCS_read_miss_cur;
	int PCS_write_hit_cur;
	int PCS_write_miss_cur;
	//
	int *PCH_areaAccess_read;
	int *PCH_areaAccess_write;
	int *PCS_areaAccess_read;
	int *PCS_areaAccess_write;
	//int PCH_areaAccess_capacity;
	//int PCS_areaAccess_capacity;
	//
	ResizeInfo(int const &pch_capacity, int const &pcs_capacity) {
		PCH_read_prev = 0;
		PCH_write_prev = 0;
		PCS_read_prev = 0;
		PCS_write_prev = 0;
		//
		PCS_dirtyRatio_prev = 0.0;
		PCH_dirtyRatio_prev = 0.0;
		SC_dirtyRatio_prev = 0.0;
		PCH_cacheRatio_prev = 0.0;
		//
		PCH_read_hit_cur = 0;
		PCH_read_miss_cur = 0;
		PCH_write_hit_cur = 0;
		PCH_write_miss_cur = 0;
		//
		PCS_read_hit_cur = 0;
		PCS_read_miss_cur = 0;
		PCS_write_hit_cur = 0;
		PCS_write_miss_cur = 0;
		//
		PCH_areaAccess_read = new int[pch_capacity] {0};
		PCH_areaAccess_write = new int[pch_capacity] {0};
		PCS_areaAccess_read = new int[pcs_capacity] {0};
		PCS_areaAccess_write = new int[pcs_capacity] {0};
		//PCH_areaAccess_capacity = pch_capacity;
		//PCS_areaAccess_capacity = pcs_capacity;
	}
	~ResizeInfo() {
		delete[] PCH_areaAccess_read;
		delete[] PCH_areaAccess_write;
		delete[] PCS_areaAccess_read;
		delete[] PCS_areaAccess_write;
	}
	int total() {
		return (PCH_read_hit_cur + PCH_read_miss_cur + PCH_write_hit_cur + PCH_write_miss_cur +
			PCS_read_hit_cur + PCS_read_miss_cur + PCS_write_hit_cur + PCS_write_miss_cur);
	}
	void reset(int const &pch_capa, int const &pcs_capa) {
		PCH_read_hit_cur = 0;
		PCH_read_miss_cur = 0;
		PCH_write_hit_cur = 0;
		PCH_write_miss_cur = 0;
		//
		PCS_read_hit_cur = 0;
		PCS_read_miss_cur = 0;
		PCS_write_hit_cur = 0;
		PCS_write_miss_cur = 0;
		//

		delete[] PCH_areaAccess_read;
		delete[] PCH_areaAccess_write;
		delete[] PCS_areaAccess_read;
		delete[] PCS_areaAccess_write;

		PCH_areaAccess_read = new int[pch_capa] {0};
		PCH_areaAccess_write = new int[pch_capa] {0};
		PCS_areaAccess_read = new int[pcs_capa] {0};
		PCS_areaAccess_write = new int[pcs_capa] {0};
		//PCH_areaAccess_capacity = pch_capa;
		//PCS_areaAccess_capacity = pcs_capa;
	}
};

class Cache {
private:
	CPageCacheHdd *pch;
	CPageCacheSsd *pcs;
	CSsdCache *sc;
	unordered_map<int, node *> map;
	//set value
	int period;
	int ssdcache_threshold;
	//resize info
	ResizeInfo *counts;
	//count value
	double usedTime = 0.0;
	double meanTssd = 0.0;
	double meanThdd = 0.0;
	int countPeriod = 0;
	int minCapacity = 0.0;
	//func
	void PCH_add(node *newnode) {
		if (pch->isFull()) {
			PCH_kick();
		}
		pch->push_front(newnode);
	}
	void PCS_add(node *newnode) {
		if (pcs->isFull() >= 1) {
			PCS_kick();
		}
		pcs->push_front(newnode);
	}
	void SC_add(node *newnode) {
		if (sc->isFull()) {
			SC_kick();
		}
		sc->push_front(newnode);
	}
	void PCH_kick() {
		node *target = pch->back();
		//kick
		pch->pop_back();
		//after kick
		if (target->hitCount > ssdcache_threshold) {
			target->hitCount = 0;
			if (target->pageCacheDirty) {
				//write to ssd
				target->ssdCacheDirty = true;
				target->pageCacheDirty = false;
				usedTime += get_ssd_time(4, true);
			}
			SC_add(target);
		}
		else {
			if (target->pageCacheDirty) {
				usedTime += get_hdd_time(4);
			}
			map.erase(target->blockNum);
			delete target;
		}
	}
	void PCS_kick() {
		node *target = pcs->back();
		//kick it
		pcs->pop_back();
		//after kick
		if (target->pageCacheDirty) {
			//write to ssd
			target->ssdCacheDirty = true;
			target->pageCacheDirty = false;
			usedTime += get_ssd_time(4, true);
		}
		//move to mru of larc list
		sc->move_to_front(target);
	}
	void SC_kick() {
		node *target = sc->back();
		//kick it
		sc->pop_back();
		//after kick
		if (target->ssdCacheDirty) {
			//flush to HDD
			usedTime += get_hdd_time(4);
		}
		if (!target->inPageCacheSSD) {
			map.erase(target->blockNum);
			delete target;
		}
	}
	void update_predict_val() {
		/*update predict value n->n+1*/
		/*dirty ratio*/
		counts->PCS_dirtyRatio_prev = (1 - ALPHA)*counts->PCS_dirtyRatio_prev + ALPHA*pcs->dirtyRatio();
		counts->PCH_dirtyRatio_prev = (1 - ALPHA)*counts->PCH_dirtyRatio_prev + ALPHA*pch->dirtyRatio();
		counts->PCH_cacheRatio_prev = (1 - ALPHA)*counts->PCH_cacheRatio_prev + ALPHA*pch->cacheRatio();
		counts->SC_dirtyRatio_prev = (1 - ALPHA)*counts->SC_dirtyRatio_prev + ALPHA*sc->dirtyRatio();
		//printf("pcs_d:%f, pch_d:%f, pch_c:%f, sc_d:%f\n", counts->PCS_dirtyRatio_prev, counts->PCH_dirtyRatio_prev, counts->PCH_cacheRatio_prev, counts->SC_dirtyRatio_prev);
		/*accesscount*/
		counts->PCH_read_prev = (1 - ALPHA)*counts->PCH_read_prev + ALPHA*(counts->PCH_read_hit_cur + counts->PCH_read_miss_cur);
		counts->PCH_write_prev = (1 - ALPHA)*counts->PCH_write_prev + ALPHA*(counts->PCH_write_hit_cur + counts->PCH_write_miss_cur);
		counts->PCS_read_prev = (1 - ALPHA)*counts->PCS_read_prev + ALPHA*(counts->PCS_read_hit_cur + counts->PCS_read_miss_cur);
		counts->PCS_write_prev = (1 - ALPHA)*counts->PCS_write_prev + ALPHA*(counts->PCS_write_hit_cur + counts->PCS_write_miss_cur);
	}
	void cal_work_time(double *Thdd, double *Tssd, int const &pch_read_total, int const &pch_write_total, int const &pcs_read_total, int const &pcs_write_total) {
		double TwriteSsd = get_ssd_time(4, true);
		double TreadSsd = get_ssd_time(4, false);
		double TrwHdd = get_hdd_time(4);
		int pch_read_missTemp,
			pch_write_missTemp,
			pcs_read_missTemp,
			pcs_write_missTemp;
		/*predict future miss count*/
		if (pch_read_total == 0)
			pch_read_missTemp = 0;
		else
			pch_read_missTemp = (int)(counts->PCH_read_prev*((double)counts->PCH_read_miss_cur / pch_read_total));
		if (pch_write_total == 0)
			pch_write_missTemp = 0;
		else
			pch_write_missTemp = (int)(counts->PCH_write_prev*((double)counts->PCH_write_miss_cur / pch_write_total));
		if (pcs_read_total == 0)
			pcs_read_missTemp = 0;
		else
			pcs_read_missTemp = (int)(counts->PCS_read_prev*((double)counts->PCS_read_miss_cur / pcs_read_total));
		if (pcs_write_total == 0)
			pcs_write_missTemp = 0;
		else
			pcs_write_missTemp = (int)(counts->PCS_write_prev*((double)counts->PCS_write_miss_cur / pcs_write_total));
		/*calculate and return*/
		double writePenalitySsd = counts->PCS_dirtyRatio_prev*TwriteSsd;
		double writePenalityHdd = counts->PCH_cacheRatio_prev*counts->SC_dirtyRatio_prev*TrwHdd + (1 - counts->PCH_cacheRatio_prev)*counts->PCH_dirtyRatio_prev*TrwHdd;
		*Tssd =
			pcs_read_missTemp*(TreadSsd + writePenalitySsd) +
			pcs_write_missTemp*writePenalitySsd +
			(pch_read_missTemp + pch_write_missTemp)*counts->PCH_cacheRatio_prev*TwriteSsd;
		*Thdd = pch_read_missTemp*(TrwHdd + writePenalityHdd) + pch_write_missTemp*writePenalityHdd;
	}
	
	void resize() {
		/*for first time*/
		if (counts->PCH_read_prev == 0) {
			counts->PCH_read_prev = counts->PCH_read_hit_cur + counts->PCH_read_miss_cur;
			counts->PCH_write_prev = counts->PCH_write_hit_cur + counts->PCH_write_miss_cur;
			counts->PCS_read_prev = counts->PCS_read_hit_cur + counts->PCS_read_miss_cur;
			counts->PCS_write_prev = counts->PCS_write_hit_cur + counts->PCS_write_miss_cur;
			//
			counts->PCS_dirtyRatio_prev = pcs->dirtyRatio();
			counts->PCH_dirtyRatio_prev = pch->dirtyRatio();
			counts->SC_dirtyRatio_prev = sc->dirtyRatio();
			counts->PCH_cacheRatio_prev = pch->cacheRatio();
		}
		/*access time*/
		double TwriteSsd = get_ssd_time(4, true);
		double TreadSsd = get_ssd_time(4, false);
		double TrwHdd = get_hdd_time(4);
		/*pch and pcs capacity*/
		int pch_capa = pch->Capacity();
		int pcs_capa = pcs->Capacity();
		int resizeCount = 0;
		/*update*/
		update_predict_val();
		/*predict total count for miss ratio*/
		int pch_read_total = counts->PCH_read_hit_cur + counts->PCH_read_miss_cur;
		int pch_write_total = counts->PCH_write_hit_cur + counts->PCH_write_miss_cur;
		int pcs_read_total = counts->PCS_read_hit_cur + counts->PCS_read_miss_cur;
		int pcs_write_total = counts->PCS_write_hit_cur + counts->PCS_write_miss_cur;
		/*cal Tssd, Thdd*/
		double Thdd, Tssd;
		cal_work_time(&Thdd, &Tssd, pch_read_total, pch_write_total, pcs_read_total, pcs_write_total);
		//printf("before: Thdd=%f, Tssd=%f\n", Thdd, Tssd);
		/*test load balance*/
		if (Thdd <= Tssd) {
			if (!(Tssd <= Thdd + TrwHdd) && pch_capa > minCapacity) {
				while (1) {
					pch_read_total -= counts->PCH_areaAccess_read[resizeCount];
					pch_write_total -= counts->PCH_areaAccess_write[resizeCount];
					pcs_read_total += counts->PCH_areaAccess_read[resizeCount];
					pcs_write_total += counts->PCH_areaAccess_write[resizeCount];
					if (pch_capa - resizeCount < minCapacity || pch_read_total < 0 || pch_write_total < 0) {
						resizeCount--;
						break;
					}
					/*cal Thdd, Tssd*/
					cal_work_time(&Thdd, &Tssd, pch_read_total, pch_write_total, pcs_read_total, pcs_write_total);
					/**/
					if (Tssd <= Thdd + TrwHdd || Thdd >= Tssd) {
						break;
					}
					resizeCount++;
				}
				//set new capacity
				pch_capa -= resizeCount;
				pcs_capa += resizeCount;
				pch->Resize(pch_capa);
				while (pch->isFull() == 2) {
					PCH_kick();
				}
				pcs->Resize(pcs_capa);
				while (pcs->isFull() == 2) {
					PCS_kick();
				}
			}
		}
		else {
			//Thdd > Tssd
			if (!(Thdd <= Tssd + (TreadSsd + TwriteSsd) / 2) && pcs_capa > minCapacity) {
				while (1) {
					pch_read_total += counts->PCS_areaAccess_read[resizeCount];
					pch_write_total += counts->PCS_areaAccess_write[resizeCount];
					pcs_read_total -= counts->PCS_areaAccess_read[resizeCount];
					pcs_write_total -= counts->PCS_areaAccess_write[resizeCount];
					/*it can't be reduce any more*/
					if (pcs_capa - resizeCount < minCapacity || pcs_read_total < 0 || pcs_write_total < 0) {
						resizeCount--;
						break;
					}
					/*calculate*/
					cal_work_time(&Thdd, &Tssd, pch_read_total, pch_write_total, pcs_read_total, pcs_write_total);
					/*test condition*/
					if (Thdd <= (Tssd + (TreadSsd + TwriteSsd) / 2) || Tssd >= Thdd) {
						break;
					}
					/*add count*/
					resizeCount++;
				}
				//set new capacity
				pch_capa += resizeCount;
				pcs_capa -= resizeCount;
				pch->Resize(pch_capa);
				while (pch->isFull() == 2) {
					PCH_kick();
				}
				pcs->Resize(pcs_capa);
				while (pcs->isFull() == 2) {
					PCS_kick();
				}
			}
		}
		//printf("after: Thdd=%f, Tssd=%f\n", Thdd, Tssd);
		meanThdd *= (countPeriod - 1);
		meanTssd *= (countPeriod - 1);
		meanThdd = (meanThdd + Thdd) / countPeriod;
		meanTssd = (meanTssd + Tssd) / countPeriod;
		/*reset*/
		counts->reset(pch_capa, pcs_capa);
	}

public:
	Cache(int const &psize, int const &ssize) {
		int pcs_size = psize / 2;
		int pch_size = psize - pcs_size;
		pch = new CPageCacheHdd(pch_size);
		pcs = new CPageCacheSsd(pcs_size);
		sc = new CSsdCache(ssize);
		//set value
		period = PERIOD;
		ssdcache_threshold = SSD_THRESHOLD;
		//
		counts = new ResizeInfo(pch_size, pcs_size);
		minCapacity = psize/50;
	}
	~Cache() {
		for (auto it = map.begin(); it != map.end(); it++) {
			delete it->second;
		}
		delete pch;
		delete pcs;
		delete sc;
		delete counts;
	}

	void request(int const &blockNum, int const &blockSize, bool const &isRead) {
		//test resize or not
		int testResize = counts->total();
		if (testResize % period == 0 && testResize != 0) {
			countPeriod++;
			resize();
			//printf("pch_capa: %d, pcs_capa: %d\n", pch->Capacity(), pcs->Capacity());
			printf("%d ", blockNum);
		}

		//
		if (!map.count(blockNum)) {//not in cache
			if (!isRead) {//write to memory
				counts->PCH_write_miss_cur++;
			}
			else {//read from hdd and write to memory
				counts->PCH_read_miss_cur++;
				usedTime += get_hdd_time(blockSize);
			}
			node *newnode = new node(blockNum);
			newnode->pageCacheDirty = !isRead;
			//add to mru of lru list
			PCH_add(newnode);
			//add to map
			map[blockNum] = newnode;
			return;
		}
		//get target
		node *target = map[blockNum];
		if (target->inPageCacheSSD) {//in Page Cache SSD
			int targetNum = pcs->number(target);
			if (targetNum == -1) {
				if (target->PCS_next || target->PCS_prev) {
					printf("number wrong\n");
				}
				else {
					printf("error: not in PageCache SSD\n");
				}
			}
			if (!isRead) {
				target->pageCacheDirty = true;
				counts->PCS_write_hit_cur++;
				counts->PCS_areaAccess_write[targetNum]++;
			}
			else {
				counts->PCS_read_hit_cur++;
				counts->PCS_areaAccess_read[targetNum]++;
			}

			//move page cache ssd and ssd cache to mru
			pcs->move_to_front(target);
			sc->move_to_front(target);
		}
		else if (target->inPageCacheHDD) {//in Page Cache HDD
			target->hitCount++;
			int targetNum = pch->number(target);
			if (targetNum == -1) {
				printf("error: not in PageCache HDD\n");
			}
			if (!isRead) {
				target->pageCacheDirty = true;
				counts->PCH_write_hit_cur++;
				counts->PCH_areaAccess_write[targetNum]++;
			}
			else {
				counts->PCH_read_hit_cur++;
				counts->PCH_areaAccess_read[targetNum]++;
			}
			//move page cache hdd to mru
			pch->move_to_front(target);
		}
		else if (target->inSsdCache) {//in SSD Cache
			if (!isRead) {
				counts->PCS_write_miss_cur++;
				//write to memory
				target->ssdCacheDirty = false;
				target->pageCacheDirty = true;
			}
			else {
				counts->PCS_read_miss_cur++;
				//read ssd and write to memory
				usedTime += get_ssd_time(blockSize, false);
			}
			//move to mru of larc list
			sc->move_to_front(target);
			//add to pageCache_ssd
			PCS_add(target);
		}
	}

	double used_time() {
		return usedTime;
	}
	void printInfo() {
		printf("\n---------------------------------------\n");
		printf("used time : %fs.\n", used_time() / 1000);
		printf("mean Tssd : %f, mean Thdd : %f\n", meanTssd, meanThdd);
		printf("pch length/capacity : %d/%d.\n", pch->len(), pch->Capacity());
		printf("pcs length/capacity : %d/%d.\n", pcs->len(), pcs->Capacity());
		printf("sc length/capacity : %d/%d.\n", sc->len(), sc->Capacity());
		printf("---------------------------------------\n");
	}
};

int count_request(unordered_map<int, bool> &countMap, string const &filePath) {
	string strinp;
	fstream file;
	file.open(filePath, ios::in);
	if (file.is_open()) {
		while (getline(file, strinp)) {
			istringstream ss(strinp);
			getline(ss, strinp, '\t');
			getline(ss, strinp, '\t');
			getline(ss, strinp, '\t');
			int temp = stoi(strinp);
			if (!countMap.count(temp)) {
				countMap[temp] = true;
			}
		}
		file.close();
		printf("%s count ok.\n", filePath.c_str());
	}
	else {
		printf("can't open file.\n");
		return -1;
	}
	return 0;
}

int access_request(Cache *test, string const &filePath) {
	string strinp;
	fstream file;
	file.open(filePath, ios::in);
	if (file.is_open()) {
		while (getline(file, strinp)) {
			int tempBlockNum;
			int tempIsRead;
			istringstream ss(strinp);
			getline(ss, strinp, '\t');//time
			getline(ss, strinp, '\t');//device num
			getline(ss, strinp, '\t');//block num
			tempBlockNum = stoi(strinp);
			getline(ss, strinp, '\t');//block ¼Æ¶q
			getline(ss, strinp, '\t');//isRead
			tempIsRead = stoi(strinp);
			getline(ss, strinp, '\t');//user

			test->request(tempBlockNum, 4, tempIsRead);
		}
		file.close();
	}
	else {
		printf("can't open file.\n");
		return -1;
	}
	return 0;
}

int main(void) {
	unordered_map<int, bool> countMap;

	/*count the number of request*/
	//count_request(countMap, "./trace/gccforssd_2_785.txt");
	//count_request(countMap, "./trace/multi1forssd.txt");
	//count_request(countMap, "./trace/cscopeforssd+24886.txt");
	//count_request(countMap, "./trace/glimpseforssd+23973.txt");
	/*request*/
	int cacheSize;
	int ssdSize;
	double time[10];
	double hitratio[10];
	Cache *test;

	for (int i = 0; i < 1; i++) {
		//cacheSize = countMap.size();
		cacheSize = 40535;
		cacheSize = (int)(cacheSize*PAGECACHE_SIZE_RATIO);
		ssdSize = cacheSize*SSDCACHE_SIZE_RATIO;
		printf("cache size = %d, %d-----------------\n", cacheSize, ssdSize);
		test = new Cache(cacheSize, ssdSize);
		//
		access_request(test, "./trace/gccforssd_2_785.txt");
		test->printInfo();
		access_request(test, "./trace/multi1forssd.txt");
		test->printInfo();
		access_request(test, "./trace/cscopeforssd+24886.txt");
		test->printInfo();
		access_request(test, "./trace/glimpseforssd+23973.txt");
		test->printInfo();
		//
		time[i] = test->used_time() / 1000;
		delete test;
		printf("end-----------------------------\n");
	}

	///*output csv file*/
	//FILE *opfile;
	//fopen_s(&opfile, "./output/1.csv", "w");
	//for (int i = 0; i < 10; i++) {
	//	fprintf(opfile, "%f\n", time[i]);
	//}
	//fclose(opfile);


	printf("all ok.\n");

	system("pause");
	return 0;
}







