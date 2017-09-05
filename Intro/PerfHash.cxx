#include "stdafx.h"
#include "PerfHash.h"

#include  <iostream>
namespace util
{

	//void mkPerfectHash(const std::set<std::wstring> &keys,std::vector<std::int32_t> &offsets)
	void mkPerfectHash(const std::set<std::wstring> &keys,std::int32_t *offsets)
	{
		size_t size=keys.size();
		//std::wcout << L"Keys size: " << size << L"\n";
		std::uint32_t item,index,i,j;
		std::int32_t d,slot;
		std::vector<std::vector<std::wstring>> buckets;
		buckets.resize(size);
		//offsets.resize(size,0x7FFFFFFF);
		std::set<std::wstring>::const_iterator iter;
		std::set<std::int32_t> occupied; // Slots that are now definitely occupied
		// 1: Place all keys in the buckets, then sort buckets by size descending
		for (iter=keys.begin();iter!=keys.end();iter++)
		{
			index=hash(iter->c_str(),iter->size(),0)%size;
			buckets[index].push_back(*iter);
		}
		std::sort( buckets.begin( ),buckets.end( ), [](const std::vector<std::wstring> &a,const std::vector<std::wstring> &b)
		{
			return a.size() > b.size();
		});
		for (i=0;i<size;++i) // we resized to that many buckets, possibly empty
		{
			if (buckets[i].size()<=1) break;
			d=1;
			std::set<std::int32_t> slots; // slots we are filling up in this iteration
			item=0;
			while(item<buckets[i].size())
			{
				index=hash(buckets[i][item].c_str(),buckets[i][item].size(),d)%size;
				if (slots.find(index)!=slots.end() || occupied.find(index)!=occupied.end())
				{	// Collision: gotta try the next d for this set
					item=0; // this makes us restart instead of continue/break
					d++;	// we're trying the next d, hoping this one will have no collisions...
					slots.clear(); // since we start over, we can forget sots we assigned this turn
				}
				else
				{
					slots.insert(index);
					item++;
				}
			}
			// Store the d that all keys in the bucket share (they map to same has with d=0)
			occupied.insert(slots.begin(),slots.end());
			//std::wcout << L"Size of buckets[" <<i<< L"]= ";
			//std::wcout << buckets[i].size() << L"\n";
			offsets[hash(buckets[i][0].c_str(),buckets[i][0].size(),0)%size]=d; // collision resolution - we see a positive value in displacement: rehash
		}
		// Only buckets with one element remain-those had no conlicts with d==0!
		std::vector<std::int32_t> free;
		free.reserve(size-i); // reserve number of unprocessed buckets...
		// Find free slots to place value in, we map from the original hash
		for (j=0;j<size;j++) if (occupied.find(j)==occupied.end()) free.push_back((std::int32_t)j);
		item=0;
		for (;i<size;i++) // is is still pointing at the first bucket with one element
		{
			if (buckets[i].empty()) break; // we allocated buckets for the case that no collisions occur...
			slot=free[item];
			item++;
			offsets[hash(buckets[i][0].c_str(),buckets[i][0].size(),0)%size]=(-slot)-1; // we see a negative value: compute offset from it.
		}
	}
}