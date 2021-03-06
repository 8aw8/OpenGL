/*
 * Created AW
 * See also lib3ds examples
 * Need packets:  glm, lib3ds-20080909
 */

#ifdef _WIN32
#define SDL_main main
#include <windows.h>
#endif

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>


#include "GL/gl.h"
#include "lib3ds.h"

#define GLM_FORCE_INLINE
#define GLM_FORCE_SSE2


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/fast_square_root.hpp>


#include "с3ds.h"
//#include "lib3ds_impl.h"

#include "math_sse/mat4.h"
#include "math_sse/vec4.h"

#include "dmemcpy.h"



vec4 cross(const vec4 &a, const vec4 &b)
{
    return a.yzxw * b.zxyw - a.zxyw * b.yzxw;
}

/*
	template <typename T> 
	GLM_FUNC_QUALIFIER detail::tmat4x4<T> scale
	(
		detail::tmat4x4<T> const & m,
		detail::tvec3<T> const & v
	)
	{
		detail::tmat4x4<T> Result(detail::tmat4x4<T>::null);
		Result[0] = m[0] * v[0];
		Result[1] = m[1] * v[1];
		Result[2] = m[2] * v[2];
		Result[3] = m[3];
		return Result;
	}

*/


mat4 scaleMatrix_sse(const mat4 &m, const vec4  &v)
{
  mat4 Result(1.0f);  
  	Result[0] = m[0] * v[0];
	Result[1] = m[1] * v[1];
	Result[2] = m[2] * v[2];
	Result[3] = m[3];  
  return Result; 
}



void matrix_dump(GLfloat matrix[16])
{
    int i;

    for (i = 0; i < 4; ++i)
    {
        std::cout << matrix[4*i] << " " << matrix[4*i+1] << " " << matrix[4*i+2] << " " << matrix[4*i+3] << " " << std::endl;
    }

    return ;
}

static long fileio_seek_func(void *self, long offset, Lib3dsIoSeek origin) {
    FILE *f = (FILE*)self;
    int o;
    switch (origin) {
    case LIB3DS_SEEK_SET:
        o = SEEK_SET;
        break;

    case LIB3DS_SEEK_CUR:
        o = SEEK_CUR;
        break;

    case LIB3DS_SEEK_END:
        o = SEEK_END;
        break;
    }
    return (fseek(f, offset, o));
}


static long fileio_tell_func(void *self) {
    FILE *f = (FILE*)self;
    return(ftell(f));
}


static size_t fileio_read_func(void *self, void *buffer, size_t size) {
    FILE *f = (FILE*)self;
    return (fread(buffer, 1, size, f));
}


static size_t fileio_write_func(void *self, const void *buffer, size_t size) {
    FILE *f = (FILE*)self;
    return (fwrite(buffer, 1, size, f));
}


static void fileio_log_func(void *self, Lib3dsLogLevel level, int indent, const char *msg)
{
    static const char * level_str[] = {
        "ERROR", "WARN", "INFO", "DEBUG"
    };
    if (log_level >=  level) {
        int i;
        printf("%5s : ", level_str[level]);
        for (i = 1; i < indent; ++i) printf("\t");
        printf("%s\n", msg);
    }
}

Lib3dsFile* Load3ds(const char* filename)
{
    FILE *file;
    Lib3dsFile *f = 0;
    Lib3dsIo io;
    int result;
    int i;

    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "***ERROR***\nFile not found: %s\n", filename);
        return NULL;

    }

    f = lib3ds_file_new();

    memset(&io, 0, sizeof(io));
    io.self = file;
    io.seek_func = fileio_seek_func;
    io.tell_func = fileio_tell_func;
    io.read_func = fileio_read_func;
    io.write_func = fileio_write_func;
    io.log_func = fileio_log_func;

    result =  lib3ds_file_read(f, &io);

    fclose(file);
    return f;
}

void mesh_dump(Lib3dsMesh *mesh) {
    int i;
    float p[3];


    assert(mesh);
    printf("  %s vertices=%i faces=%i\n",
           mesh->name,
           mesh->nvertices,
           mesh->nfaces);
    printf("  matrix:\n");

    //  GLdouble matrix = (GLdouble[16] )mesh->matrix;
    // matrix_dump(&matrix);

    printf("  vertices (x, y, z, u, v):\n");
    for (i = 0; i < mesh->nvertices; ++i) {
        lib3ds_vector_copy(p, mesh->vertices[i]);

        printf("    %10.5f %10.5f %10.5f", p[0], p[1], p[2]);
        if (mesh->texcos) {
            printf("%10.5f %10.5f", mesh->texcos[i][0], mesh->texcos[i][1]);
        }
        printf("\n");

    }
    printf("  facelist:\n");
    for (i = 0; i < mesh->nfaces; ++i) {
        printf("    %4d %4d %4d  flags:%X  smoothing:%X  material:\"%d\"\n",
               mesh->faces[i].index[0],
               mesh->faces[i].index[1],
               mesh->faces[i].index[2],
               mesh->faces[i].flags,
               mesh->faces[i].smoothing_group,
               mesh->faces[i].material
              );
    }
}

GLuint GL_DrawMesh(Lib3dsMesh *mesh)
{

    GLuint ls_mesh;

    assert(mesh);

    glm::mat4 E(1.0f);
    glm::vec4 vres(0,0,0,0);
    glm::vec3 a(0.0f);//  = glm::make_vec4(mesh->vertices[j]);
    glm::vec3 b(0.0f);//  = glm::make_vec4(mesh->vertices[j]);
    glm::vec3 c(0.0f);//  = glm::make_vec4(mesh->vertices[j]);
    glm::vec3 normal(0.0f);//  = glm::make_vec4(mesh->vertices[j]);
    glm::vec3 v(0.0f,0.0f,1.0f);

    GLfloat *vertex  = new GLfloat[mesh->nvertices*3];
    GLfloat *colors  = new GLfloat[mesh->nvertices*3];
    GLfloat *norms   = new GLfloat[mesh->nvertices*3];
    GLuint *indices  = new GLuint[mesh->nfaces*3];


    glm::mat4 D = glm::scale(E, glm::vec3(0.003f, 0.003f, 0.003f));

    for (int j = 0; j < mesh->nvertices; ++j)
    {
        glm::vec4 v =  glm::make_vec4(mesh->vertices[j]);
        vres = v * D;

        mesh->vertices[j][0] = vres[0];
        mesh->vertices[j][1] = vres[1];
        mesh->vertices[j][2] = vres[2];

        vertex[3*j] = vres[0];
        vertex[3*j+1] = vres[1];
        vertex[3*j+2] = vres[2];

        colors[3*j] = 0.0f;
        colors[3*j+1] = 0.0f;
        colors[3*j+2] = 0.0f;
    }

    for (int i = 0; i < mesh->nfaces; ++i)
    {
        int index0 = mesh->faces[i].index[0];
        int index1 = mesh->faces[i].index[1];
        int index2 = mesh->faces[i].index[2];

        indices[3*i]   = index0;
        indices[3*i+1] = index1;
        indices[3*i+2] = index2;

        a =  glm::make_vec3(mesh->vertices[index0]);
        b =  glm::make_vec3(mesh->vertices[index1]);
        c =  glm::make_vec3(mesh->vertices[index2]);

        normal = glm::fastNormalize(glm::cross(c - a, b - a));

        norms[3*index0]   = normal[0]*(-1);
        norms[3*index0+1] = normal[1]*(-1);
        norms[3*index0+2] = normal[2]*(-1);

        //norms[3*index1]   = normal[0]*(-1);
        //norms[3*index1+1] = normal[1]*(-1);
        //norms[3*index1+2] = normal[2]*(-1);

        //norms[3*index2]   = normal[0]*(-1);
        //norms[3*index2+1] = normal[1]*(-1);
        //norms[3*index2+2] = normal[2]*(-1);
        if (glm::dot(normal, v) > 0)
        {
            indices[3*i] =0;
            indices[3*i+1] =0;
            indices[3*i+2] =0;
        }
    }

    ls_mesh=glGenLists(1);

    glNewList(ls_mesh,GL_COMPILE);

    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (mesh->texcos != NULL)   glEnableClientState(GL_TEXTURE_COORD_ARRAY );

    glNormalPointer(GL_FLOAT, 0, norms);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glVertexPointer(3, GL_FLOAT, 0, mesh->vertices); //glVertexPointer(3, GL_FLOAT, 0, vertex);
    if (mesh->texcos != NULL)   glTexCoordPointer(2, GL_FLOAT, 0, mesh->texcos);

    glDrawElements(GL_TRIANGLES, mesh->nfaces*3, GL_UNSIGNED_INT , indices);

    if (mesh->texcos != NULL) glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);


    glEndList();

    delete [] norms;
    delete [] vertex;
    delete [] colors;
    delete [] indices;

    return ls_mesh;
}

GLuint GL_DrawMesh_SSE(Lib3dsMesh *mesh)
{

    GLuint ls_mesh;
    GLuint render_nfaces=0;

    assert(mesh);

    GLfloat *vertex  = new GLfloat[mesh->nvertices*3];
    GLfloat *colors  = new GLfloat[mesh->nvertices*3];
    GLfloat *norms   = new GLfloat[mesh->nvertices*3];
    GLuint *indices = new GLuint[mesh->nfaces*3];
    GLuint *render_index = new GLuint[mesh->nfaces*3];
  
    mat4 E(1.0f);
    mat4 S = scaleMatrix_sse(E, vec4(0.003f, 0.003f, 0.003f,0.0f));
    vec4 view_vector(0.0f,0.0f,1.0f,0.0f);
    
    
    mat4 M((GLfloat *)mesh->matrix);
    
    S =  S * M;
    
 /*   
    mat4 M(Mt[0],Mt[1],Mt[2],Mt[3],
           Mt[4],Mt[5],Mt[6],Mt[7],
	   Mt[8],Mt[9],Mt[10],Mt[11],
	   Mt[12],Mt[13],Mt[14],Mt[15]	   
	  );
    
    //mat4 M(  );
 */  
   
    std::cout << "SSE" << std::endl;
    
    for (int i = 0; i < 4; ++i)
    {
        std::cout << M[i][0] << " " << M[i][1] << " " << M[i][2] << " " << M[i][3] << " " << std::endl;	
    }
    
    GLfloat temp0[4] = {0.0f,0.0f,0.0f,0.0f};
    GLfloat temp1[4] = {0.0f,0.0f,0.0f,0.0f};
    GLfloat temp2[4] = {0.0f,0.0f,0.0f,0.0f};
    GLfloat    ve[4] = {0.0f,0.0f,0.0f,0.0f};
    GLfloat minus = -1.0f;
    GLfloat meshColor = mesh->color/255.0f;
    
    
    for (int i = 0; i < mesh->nvertices; ++i)
    {
        nmemcpy(temp0, mesh->vertices[i], 3*sizeof(GLfloat));
      
        vec4 v(temp0);
        
	v = v * S;

        mesh->vertices[i][0] = v[0];
        mesh->vertices[i][1] = v[1];
        mesh->vertices[i][2] = v[2];

        vertex[3*i]   = v[0];
        vertex[3*i+1] = v[1];
        vertex[3*i+2] = v[2];

        colors[3*i]   = meshColor;
        colors[3*i+1] = meshColor;
        colors[3*i+2] = meshColor;
    }

 

    for (int i = 0; i < mesh->nfaces; ++i)
    {
        int index0 = mesh->faces[i].index[0];
        int index1 = mesh->faces[i].index[1];
        int index2 = mesh->faces[i].index[2];

        indices[3*i]   = index0;
        indices[3*i+1] = index1;
        indices[3*i+2] = index2;

        nmemcpy(temp0, mesh->vertices[index0], 3*sizeof(GLfloat));
        nmemcpy(temp1, mesh->vertices[index1], 3*sizeof(GLfloat));
        nmemcpy(temp2, mesh->vertices[index2], 3*sizeof(GLfloat));

        vec4 va(temp0);
        vec4 vb(temp1);
        vec4 vc(temp2);

        vec4 vn = normalize(cross(vc - va, vb - va))*minus;
	
	if (dot(vn, view_vector)>0) 
	{
	  render_index[3*render_nfaces]   = index0;
	  render_index[3*render_nfaces+1] = index1;
	  render_index[3*render_nfaces+2] = index2;
	  
	  render_nfaces++;
	}

        norms[3*index0]   = vn[0];
        norms[3*index0+1] = vn[1];
        norms[3*index0+2] = vn[2];
    }//for (int i = 0; i < mesh->nfaces; ++i)

    std::cout << render_nfaces << "/" << mesh->nfaces <<  std::endl; 
    
    ls_mesh=glGenLists(1);

  glNewList(ls_mesh,GL_COMPILE);

    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (mesh->texcos != NULL)   glEnableClientState(GL_TEXTURE_COORD_ARRAY );

    glNormalPointer(GL_FLOAT, 0, norms);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glVertexPointer(3, GL_FLOAT, 0, vertex); //glVertexPointer(3, GL_FLOAT, 0, vertex);
    if (mesh->texcos != NULL)   glTexCoordPointer(2, GL_FLOAT, 0, mesh->texcos);

   if (render_index != NULL) glDrawElements(GL_TRIANGLES, render_nfaces*3, GL_UNSIGNED_INT , render_index);
    
   //glDrawElements(GL_TRIANGLES, mesh->nfaces*3, GL_UNSIGNED_INT , indices);

    if (mesh->texcos != NULL) glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);


  glEndList();

    delete [] norms;
    delete [] vertex;
    delete [] colors;
    delete [] indices;

    return ls_mesh;


}




