package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"runtime/pprof"
	"strconv"
	"time"
)

const DAMP = 0.85

var itt = 20

type page struct {
	id             int
	incomming_rank float32
	edges          []int //id's of other nodes
	rank           float32
}

func newPage(pid int) *page {
	return &page{id: pid, incomming_rank: 0.0, edges: make([]int, 0), rank: 1.0}
}

func (p *page) String() string {
	var s string
	s += fmt.Sprintf("Id: %d ", p.id)
	s += fmt.Sprintf("I-Rank: %f ", p.incomming_rank)
	s += "Edges: ["
	for _, e := range p.edges {
		s += fmt.Sprintf("%d,", e)
	}
	s += "] "
	s += fmt.Sprintf("Rank :%f\n", p.rank)
	return s
}

var pages map[int]*page
var apages []*page
var ids []int

func main() {
	f, err := os.Create("cpu.prof")
	if err != nil {
		log.Fatal("could not create CPU profile: ", err)
	}
	if err := pprof.StartCPUProfile(f); err != nil {
		log.Fatal("could not start CPU profile: ", err)
	}
	defer pprof.StopCPUProfile()
	fmt.Println("vim-go")
	pages = make(map[int]*page, 0)
	parseFile("../web-Google.txt")
	//parseFile("../tiny.txt")
	fmt.Println("parsed")
	start := time.Now()
	pageRank(itt)
	dir := time.Now().Sub(start)
	fmt.Printf("Rounds %d Duration %f\n", itt, dir.Seconds())

	//for i := range pages {
	//	fmt.Print((*pages[i]).String())
	//}
}

func pageRank(rounds int) {
	for i := 0; i < rounds; i++ {
		for j := 0; j < len(ids); j++ {
			sendMessages(ids[j])
		}
		for j := 0; j < len(ids); j++ {
			update(ids[j])
		}
	}
}

func sendMessages(id int) {
	var outrank float32
	var p *page
	p = apages[id]
	outrank = p.rank / float32((len(p.edges) + 1))
	for i := 0; i < len(p.edges); i++ {
		apages[p.edges[i]].incomming_rank += outrank
	}
}

func update(id int) {
	apages[id].rank = (1-DAMP)*(1.0/float32(len(pages))) + (DAMP * apages[id].incomming_rank)
}

func parseFile(filename string) {
	f, err := os.Open(filename)
	if err != nil {
		log.Fatalf("%s", err.Error)
	}
	defer f.Close()
	scanner := bufio.NewScanner(f)

	var i int
	var buf string
	var max int
	for scanner.Scan() {
		buf = scanner.Text()
		//fmt.Println(buf)

		for i = 0; i < len(buf); i++ {
			if buf[i] == '\t' {
				a, err := strconv.Atoi(buf[0:i])
				if err != nil {
					log.Fatal(err)
				}
				b, err := strconv.Atoi(buf[i+1 : len(buf)])
				if err != nil {
					log.Fatal(err)
				}
				if _, ok := pages[a]; !ok {
					pages[a] = newPage(a)
				}
				if _, ok := pages[b]; !ok {
					pages[b] = newPage(b)
				}
				if a > max {
					max = a
				}
				if b > max {
					max = b
				}
				pages[a].edges = append(pages[a].edges, b)
				pages[b].incomming_rank = 0.0
			}
		}
	}
	fmt.Printf("Max = %d\n", max)
	apages = make([]*page, max+1)
	ids = make([]int, len(pages))
	j := 0
	for i := range pages {
		apages[pages[i].id] = pages[i]
		ids[j] = pages[i].id
		j++
	}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
}

/*
type pageHash [][]*page

func NewPageHash(size int) *pageHash {
	p := make([][]*page, size)
	logsize := math.Sqrt(float32(size))
	for i := range p {
		p[i] = make([]*page, int(logsize))
	}
	return *pageHash(p)
}

func (ph *pageHash) Put(id int) {
	var l1 int
	var l2 int
*/
