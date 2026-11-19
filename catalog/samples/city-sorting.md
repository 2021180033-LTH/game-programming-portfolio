# 도시 데이터 정렬 · C와 C++ 소스 발췌

같은 도시 자료를 이름순과 y 좌표순으로 정렬하는 두 제출물의 비교 함수다.

## C · qsort에 전달한 비교 함수

```c
int City_compare_name(const void *a, const void *b){
    City* c1 = (City*)a;
    City* c2 = (City*)b;

    return strcmp(c1->name, c2->name);
}

int City_compare_y(const void *a, const void *b){
    City* c1 = (City*)a;
    City* c2 = (City*)b;

    // if(c1->y<c2->y) return -1;
    // if(c1->y>c2->y) return 1;
    // if(c1->y==c2->y) return 0;

    return c1->y - c2 -> y;
}
```

## C++ · sort에 전달한 y 좌표 비교 함수

```cpp
bool City_compare_y(const City &a, const City &b)
{
    return a.y < b.y;
}
```

이름 정렬은 C 코드에서 `strcmp`를 호출하고, C++ 코드에서는 클래스의 순서 비교를 사용하는 `sort(cities, cities+n_cities)`를 호출한다.
