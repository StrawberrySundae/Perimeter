#ifndef PERIMETER_DRAWBUFFER_H
#define PERIMETER_DRAWBUFFER_H

class DrawBuffer {
private:
    bool LockSetup(size_t vertices, size_t indices);
public:
    ePrimitiveType primitive;
    VertexBuffer vb;
    IndexBuffer ib;
    bool dynamic = false;
    size_t locked_vertices = 0;
    size_t locked_indices = 0;
    size_t lock_written_vertices = 0;
    size_t lock_written_indices = 0;
    size_t written_vertices = 0;
    size_t written_indices = 0;
    
    DrawBuffer() = default;
    ~DrawBuffer() {
        Destroy();
    }
    NO_COPY_CONSTRUCTOR(DrawBuffer)

    void Create(size_t vertices, size_t indices, vertex_fmt_t fmt, bool dynamic, ePrimitiveType primitive);
    void Recreate();
    void Destroy();
    void Unlock();
    void Draw();
    void PostDraw();

    template<class TVERTEX>
    void Lock(size_t vertices, size_t indices, TVERTEX*& vertex_buf, indices_t*& indices_buf, bool increment_written) {
        //Make sure we are not locking with wrong vertex format
        xassert(TVERTEX::fmt == vb.fmt);
        xassert(!locked_vertices && !locked_indices);
        if (!LockSetup(vertices, indices)) {
            vertex_buf = nullptr;
            indices_buf = nullptr;
            return;
        }
        vertex_buf = static_cast<TVERTEX*>(gb_RenderDevice->LockVertexBuffer(vb, written_vertices, vertices));
        if (vertex_buf) locked_vertices = vertices;
        indices_buf = gb_RenderDevice->LockIndexBuffer(ib, written_indices, indices);
        if (indices_buf) locked_indices = indices;
        if (increment_written) {
            lock_written_vertices = locked_vertices;
            lock_written_indices = locked_indices;
        }
    }
    
    //Helpers for common operation

    template<class TVERTEX>
    void LockAll(TVERTEX*& vertex_buf, indices_t*& indices_buf) {
        Lock(vb.NumberVertex, ib.NumberIndices, vertex_buf, indices_buf, true);
    }

    template<class TVERTEX>
    inline TVERTEX* LockQuad(size_t quads) {
        TVERTEX* vptr;
        indices_t* iptr;
        Lock(quads * 4, quads * 6, vptr, iptr, false);
        TVERTEX* empty = nullptr;
        WriteQuad(quads, empty, iptr);
        return vptr;
    }

    template<class TVERTEX>
    inline TVERTEX* LockTriangleStrip(size_t quads, size_t rows = 1) {
        TVERTEX* vptr;
        indices_t* iptr;
        Lock(quads * 4, quads * 4, vptr, iptr, false);
        TVERTEX* empty = nullptr;
        WriteTriangleStrip(quads, rows, empty, iptr);
        return vptr;
    }
    
    //Write* functions prewrite indices and advance vertex buffer automatically

    template<class TVERTEX>
    inline void WriteQuad(size_t quads, TVERTEX*& vertex_buf, indices_t*& indices_buf) {
        xassert(primitive == PT_TRIANGLES);
        if (!quads) return;
        xassert(lock_written_vertices + quads * 4 <= locked_vertices);
        xassert(lock_written_indices + quads * 6 <= locked_indices);
        for (int i = 0; i < quads; ++i) {
            size_t b = written_vertices + lock_written_vertices;
            indices_buf[0] = b;
            indices_buf[1] = indices_buf[4] = b + 1;
            indices_buf[2] = indices_buf[3] = b + 2;
            indices_buf[5] = b + 3;
            indices_buf += 6;
            lock_written_indices += 6;
            lock_written_vertices += 4;
        }
        if (vertex_buf) vertex_buf += quads * 4;
    }

    template<class TVERTEX>
    inline void WriteTriangleStrip(size_t quads, size_t rows, TVERTEX*& vertex_buf, indices_t*& indices_buf) {
        xassert(primitive == PT_TRIANGLESTRIP);
        if (!quads) return;
        size_t line_points = quads + 1;
        size_t points = line_points * (rows + 1);
        xassert(lock_written_vertices + points <= locked_vertices);
        xassert(lock_written_indices + points <= locked_indices);
        for (int y = 0; y <= rows; ++y) {
            size_t b = written_vertices + lock_written_vertices;
            if (0 < y) {
                indices_buf[0] = b;
                indices_buf += 1;
            }
            for (int x = 0; x < quads; ++x) {
                indices_buf[0] = b + x;
                indices_buf[1] = b + x + line_points;
                indices_buf += 2;
            }
            lock_written_indices += line_points;
            lock_written_vertices += line_points;
            indices_buf[0] = written_vertices + lock_written_vertices - 1;
            indices_buf += 1;
        }
        if (vertex_buf) vertex_buf += points;
    }
};

#endif //PERIMETER_DRAWBUFFER_H
